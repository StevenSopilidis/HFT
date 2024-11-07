#include "market_data_consumer.h"


namespace Trading {
    MarketDataConsumer::MarketDataConsumer(Common::ClientId clientId, Exchange::MEMarketUpdateLFQueue *marketUpdates, const std::string &iface,
            const std::string &snapshopIp, int snapshotPort,
            const std::string &incrementalIp, int incrementalPort) : _incomingMDUpdates(marketUpdates), _logger("trading_market_data_consumer" + std::to_string(clientId) + ".log"),
            _run(false), _incrementalMcastSocket(_logger), _snapshotMcastSocket(_logger), _iface(iface), _snapshotIp(snapshopIp), _snapshotPort(snapshotPort) 
    {
        auto recv_callback = [this](auto socket) {
            recvCallback(socket);
        };

        _incrementalMcastSocket.recv_callback = recv_callback;

        // create socket to receive incremental updates and join multicast stream
        ASSERT(_incrementalMcastSocket.init(incrementalIp, iface, incrementalPort, true) >= 0, "Unable to create incremental mcast socket. error:" + std::string(std::strerror(errno)));
        ASSERT(_incrementalMcastSocket.join(incrementalIp), "Join failed on:" + std::to_string(_incrementalMcastSocket.socketFd) + " error:" + std::string(std::strerror(errno)));
    
        _snapshotMcastSocket.recv_callback = recv_callback;
    }
    
    MarketDataConsumer::~MarketDataConsumer() {
        stop();
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(5s);
    }

    auto MarketDataConsumer::stop() noexcept -> void {
        _run = false;
    }


    auto MarketDataConsumer::start() noexcept -> void {
        _run = true;
        ASSERT(Common::createAndStartThread(-1, "Trading/MarketDataConsumer", [this]() { run(); }) != nullptr, "Failed to start MarketData thread.");
    }

    auto MarketDataConsumer::run() noexcept -> void {
        _logger.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr));
        while(_run) {
            _incrementalMcastSocket.sendAndRecv();
            _snapshotMcastSocket.sendAndRecv();
        }
    }

    auto MarketDataConsumer::recvCallback(McastSocket *socket) noexcept -> void {
        const auto isSnapshot = (socket->socketFd == _snapshotMcastSocket.socketFd);
        
        if (UNLIKELY(isSnapshot && !_inRecovery)) {
            socket->next_recv_valid_index = 0;
            _logger.log("%:% %() % WARN Not expecting snapshot messages.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr));
            return;
        }

        // iterate through MDPMarketUpdates
        if (socket->next_recv_valid_index >= sizeof(Exchange::MDPMarketUpdate)) {
            size_t i = 0;
            for (; i + sizeof(Exchange::MDPMarketUpdate) <= socket->next_recv_valid_index; i += sizeof(Exchange::MDPMarketUpdate)) {
                auto request = reinterpret_cast<const Exchange::MDPMarketUpdate*>(socket->recv_buffer.data() + i);
                _logger.log("%:% %() % Received % socket len:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), (isSnapshot ? "snapshot" : "incremental"), sizeof(Exchange::MDPMarketUpdate), request->toString());
            
                // check if need to go into recovery mode
                const bool alreadyInRecovery = _inRecovery;
                _inRecovery = (alreadyInRecovery || request->seqNumber != _nextExpIncSeqNum);
            
                // if packet was dropped start recovery process if no already started
                if(UNLIKELY(_inRecovery)) {
                    if (UNLIKELY(!alreadyInRecovery))  {
                        _logger.log("%:% %() % Packet drops on % socket. SeqNum expected:% received:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), (isSnapshot ? "snapshot" : "incremental"), _nextExpIncSeqNum, request->seqNumber);
                        startSnapshotSync();
                    }

                    queueMessage(isSnapshot, request); // queue messages while on recovery
                } else if (!isSnapshot) { // received regular market update (not on recovery)
                    _logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), request->toString());
                    ++_nextExpIncSeqNum;

                    auto nextWrite = _incomingMDUpdates->getNextWriteTo();
                    *nextWrite = std::move(request->meMarketUpdate);
                    _incomingMDUpdates->updateWriteIndex();
                }
            }

            // shift data in buffer and adjust its size
            memcpy(socket->recv_buffer.data(), socket->recv_buffer.data() + i, socket->next_recv_valid_index - i);
            socket->next_recv_valid_index -= i;
        }
    }

    auto MarketDataConsumer::startSnapshotSync() noexcept -> void {
        _snapshotQueuedMsgs.clear();
        _incrementalQueuedMsgs.clear();

        ASSERT(_snapshotMcastSocket.init(_snapshotIp, _iface, _snapshotPort, true) >= 0, "Unable to create snapshot mcast socket. Error: " + std::string(std::strerror(errno)));
        ASSERT(_snapshotMcastSocket.join(_snapshotIp) >= 0, "Join failed on: " + std::to_string(_snapshotMcastSocket.socketFd) + " error: " + std::string(std::strerror(errno)));
    }

    auto MarketDataConsumer::queueMessage(bool isSnapshot, const Exchange::MDPMarketUpdate *request) -> void {
        if (isSnapshot) {
            // check if there was packet drop in snapshot socket (recived message 2nd time)
            if (_snapshotQueuedMsgs.find(request->seqNumber) != _snapshotQueuedMsgs.end()) {
                _logger.log("%:% %() % Packet drops on snapshot socket. Received for a 2nd time:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), request->toString());
                _snapshotQueuedMsgs.clear();
            }

            _snapshotQueuedMsgs[request->seqNumber] = request->meMarketUpdate;
        } else {
            _incrementalQueuedMsgs[request->seqNumber] = request->meMarketUpdate;
        }

        _logger.log("%:% %() % size snapshot:% incremental:% % => %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), _snapshotQueuedMsgs.size(), _incrementalQueuedMsgs.size(), request->seqNumber, request->toString());
        checkSnapshotSync();
    }

    auto MarketDataConsumer::checkSnapshotSync() noexcept -> void {
        if (_snapshotQueuedMsgs.empty())
            return;
        
        // check that we have a SNAPSHOT_START message
        const auto& firstSnapshotMsg = _snapshotQueuedMsgs.begin()->second;
        if (firstSnapshotMsg.type != Exchange::MarketUpdateType::SNAPSHOT_END) {
            _logger.log("%:% %() % Returning because have not seen a SNAPSHOT_START yet.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr));
            _snapshotQueuedMsgs.clear();
            return;
        }

        // check that there is not gap between sequence numbers of snapshot msgs
        std::vector<Exchange::MEMarketUpdate> finalEvents;
        auto haveCompleteSnapshot = true;
        size_t nextSnapshotSeq = 0;
        for (auto& snapshotItr : _snapshotQueuedMsgs) {
            _logger.log("%:% %() % % => %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), snapshotItr.first, snapshotItr.second.toString());
            if (snapshotItr.first != nextSnapshotSeq) {
                // GAP detected between sequence numbers
                haveCompleteSnapshot = false;
                _logger.log("%:% %() % Detected gap in snapshot stream expected:% found:% %.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), nextSnapshotSeq, snapshotItr.first, snapshotItr. second.toString());
                break;
            }

            if (snapshotItr.second.type != Exchange::MarketUpdateType::SNAPSHOT_START && snapshotItr.second.type != Exchange::MarketUpdateType::SNAPSHOT_END)
                finalEvents.push_back(snapshotItr.second);

            nextSnapshotSeq++;
        }

        if(!haveCompleteSnapshot) {
            _logger.log("%:% %() % Returning because found gaps in snapshot stream.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr));
            _snapshotQueuedMsgs.clear();
            return;
        }

        // make sure that the last msg was a SNAPSHOT_END
        const auto& lastSnapshotMsg = _snapshotQueuedMsgs.rbegin()->second;
        if (lastSnapshotMsg.type != Exchange::MarketUpdateType::SNAPSHOT_END) {
            _logger.log("%:% %() % Returning because have not seen a SNAPSHOT_END yet.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr));
            return;
        }

        // inspect incremental messages to make sure no gaps are detected
        auto haveCompleteIncremental= true;
        size_t numIncrementals = 0;
        _nextExpIncSeqNum = lastSnapshotMsg.orderId + 1;

        for (auto incItr = _incrementalQueuedMsgs.begin(); incItr != _incrementalQueuedMsgs.end(); ++incItr) {
            _logger.log("%:% %() % Checking next_exp:% vs. seq:% %.\n", __FILE__, __LINE__, __FUNCTION__,Common::getCurrentTimeStr(&_timeStr), _nextExpIncSeqNum, incItr->first, incItr->second.toString());

            // we dont care about those msgs
            if (incItr->first < _nextExpIncSeqNum)
                continue;
            
            if (incItr->first != _nextExpIncSeqNum) {
                // GAP detected
                _logger.log("%:% %() % Detected gap in incremental stream expected:% found:% %.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), _nextExpIncSeqNum, incItr-> first, incItr->second.toString());
                haveCompleteIncremental = false;
                break;
            }

            _logger.log("%:% %() % % => %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), incItr->first, incItr->second.toString());
            if (incItr->second.type != Exchange::MarketUpdateType::SNAPSHOT_START && incItr->second.type != Exchange::MarketUpdateType::SNAPSHOT_END) {
                finalEvents.push_back(incItr->second);
                _nextExpIncSeqNum++;
                numIncrementals++;
            }
        }   

        if (!haveCompleteIncremental) {
            _logger.log("%:% %() % Returning because have gaps in queued incrementals.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr)); 
            _snapshotQueuedMsgs.clear();
            return;
        }

        // write changes to trading engine
        for (const auto& itr : finalEvents) {
            auto nextWrite = _incomingMDUpdates->getNextWriteTo();
            *nextWrite = itr;
            _incomingMDUpdates->updateWriteIndex();
        }

        // recovery was successfull
        _logger.log("%:% %() % Recovered % snapshot and % incremental orders.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), _snapshotQueuedMsgs.size() - 2, numIncrementals);
        _snapshotQueuedMsgs.clear();
        _incrementalQueuedMsgs.clear();
        _inRecovery = false;
        _snapshotMcastSocket.leave(_snapshotIp, _snapshotPort);
    }
}