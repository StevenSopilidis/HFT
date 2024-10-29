#include "market_data_publisher.h"

namespace Exchange {
    MarketDataPublisher::MarketDataPublisher(MEMarketUpdateLFQueue* marketUpdates, const std::string& iface, const std::string& snapshotIp, int snapshotPort, const std::string& incrementalIp, int incrementalPort) : 
    _outgoingMdUpdates(marketUpdates), _snapshotMdUpdates(ME_MAX_MARKET_UPDATES), _run(false),
    _logger("exchange_market_data_publisher.log"), _incrementalSocket(_logger)
    {
        ASSERT(_incrementalSocket.init(incrementalIp, iface, incrementalPort, false) >= 0, "Unable to create incremental mcast socket. error: " + std::string(std::strerror(errno)));
        _snapshotSynthesizer = new SnapshotSynthesizer(&_snapshotMdUpdates, iface, snapshotIp, snapshotPort);
    }

    MarketDataPublisher::~MarketDataPublisher() {
        stop();
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);                        
        delete _snapshotSynthesizer;
        _snapshotSynthesizer = nullptr;
    }

    auto MarketDataPublisher::start() noexcept -> void {
        _run = true;
        ASSERT(Common::createAndStartThread(-1, "Exchange/MarketDataPublisher", [this](){run();}) != nullptr, "Failed to start marketDataPublisher thread");
        _snapshotSynthesizer->start();
    }

    auto MarketDataPublisher::stop() noexcept -> void {
        _run = false;
        _snapshotSynthesizer->stop();
    }

    auto MarketDataPublisher::run() noexcept -> void {
        _logger.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr));
        while(_run) {
            for (auto marketUpdate = _outgoingMdUpdates->getNextRead(); _outgoingMdUpdates->size() && marketUpdate; marketUpdate = _outgoingMdUpdates->getNextRead()) {
                _logger.log("%:% %() % Sending seq:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), _nextIncSeqNum, marketUpdate->toString().c_str());

                // send outgoing market updates
                _incrementalSocket.send(&_nextIncSeqNum, sizeof(_nextIncSeqNum));
                _incrementalSocket.send(marketUpdate, sizeof(MEMarketUpdate));
                _outgoingMdUpdates->updateReadIndex();

                // send update to snapshotSynthesizer
                auto nextWrite = _snapshotMdUpdates.getNextWriteTo();
                nextWrite->seqNumber = _nextIncSeqNum;
                nextWrite->meMarketUpdate = *marketUpdate;
                _snapshotMdUpdates.updateWriteIndex();

                _nextIncSeqNum++;
            }

            _incrementalSocket.sendAndRecv();
        }
    }


}