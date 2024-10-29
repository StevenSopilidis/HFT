#include "snapshot_synthesizer.h"

namespace Exchange {
    SnapshotSynthesizer::SnapshotSynthesizer(MDPMarketUpdateLFQueue* marketUpdates, const std::string& iface, const std::string& snapshotIp, const int snapshotPort) 
    : _snapshotMdUpdates(marketUpdates), _logger("exchange_snapshot_synthesizer.log"), _snapshotSocket(_logger), _orderPool(ME_MAX_ORDER_IDS)
    { 
        ASSERT(_snapshotSocket.init(snapshotIp, iface, snapshotPort, false) >= 0, "Unable to create mcast socket. Error: " + std::string(std::strerror(errno)));
    }

    SnapshotSynthesizer::~SnapshotSynthesizer() {
        stop();
    }

    auto SnapshotSynthesizer::start() noexcept -> void {
        _run = true;
        ASSERT(Common::createAndStartThread(-1, "Exchange/SnapshotSynthesizer", [this](){run();}) != nullptr,  "Failed to start SnapshotSynthesizer thread.");
    }

    auto SnapshotSynthesizer::stop() noexcept -> void {
        _run = false;
    }

    auto SnapshotSynthesizer::addToSnapshot(const MDPMarketUpdate* marketUpdate) {
        const auto& meMarketUpdate = marketUpdate->meMarketUpdate;
        auto *orders = &_tickerOrders.at(meMarketUpdate.tickerId);

        switch (meMarketUpdate.type)
        {
        case MarketUpdateType::ADD: {
            auto order = orders->at(meMarketUpdate.orderId);
            ASSERT(order == nullptr, "Received: " + meMarketUpdate.toString() + " but order already exists: " + (order? order->toString() : ""));
            orders->at(meMarketUpdate.orderId) = _orderPool.allocate(meMarketUpdate);
        }
            break;
        case MarketUpdateType::MODIFY: {
            auto order = orders->at(meMarketUpdate.orderId);
            ASSERT(order != nullptr, "Received: " + meMarketUpdate.toString() + " but order does not exist.");
            ASSERT(order->orderId == meMarketUpdate.orderId, "Expecting existing order to match new one.");
            ASSERT(order->side == meMarketUpdate.side, "Expecting existing order to match new one.");
            order->qty = meMarketUpdate.qty;
            order->price = meMarketUpdate.price;
        }
            break;
        case MarketUpdateType::CANCEL: {
            auto order = orders->at(meMarketUpdate.orderId);
            ASSERT(order != nullptr, "Received: " + meMarketUpdate.toString() + " but order does not exist.");
            ASSERT(order->orderId == meMarketUpdate.orderId, "Expecting existing order to match new one.");
            ASSERT(order->side == meMarketUpdate.side, "Expecting existing order to match new one.");

            _orderPool.deallocate(order);
            orders->at(meMarketUpdate.orderId) = nullptr;
        }
            break;
        case MarketUpdateType::SNAPSHOT_START:
        case MarketUpdateType::CLEAR:
        case MarketUpdateType::SNAPSHOT_END:
        case MarketUpdateType::TRADE:
        case MarketUpdateType::INVALID:
            break;
        }
    
        ASSERT(marketUpdate->seqNumber == _lastIncSeqNum + 1, "Expected incremental seq_nums to increase.");
        _lastIncSeqNum = marketUpdate->seqNumber;
    }

    auto SnapshotSynthesizer::publishSnapshot() noexcept -> void {
        size_t snapshotSize = 0;
        const MDPMarketUpdate startMarketUpdate{snapshotSize++, {MarketUpdateType::SNAPSHOT_START, _lastIncSeqNum}};

        // send snapshot initialization
        _logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), startMarketUpdate.toString()); 
        _snapshotSocket.send(&startMarketUpdate, sizeof(MDPMarketUpdate));

        for (size_t tickerId = 0; tickerId < _tickerOrders.size(); tickerId++) {
            const auto& orders = _tickerOrders.at(tickerId);
            MEMarketUpdate meMarketUpdate;
            meMarketUpdate.type = MarketUpdateType::CLEAR;
            meMarketUpdate.tickerId = tickerId;
            
            // send clear message
            const MDPMarketUpdate clearMarketUpdate{snapshotSize++, meMarketUpdate};
            _logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), clearMarketUpdate.toString());
            _snapshotSocket.send(&clearMarketUpdate, sizeof(MDPMarketUpdate));

            // send all orders of each ticker that are live
            for (const auto order : orders) {
                if (order) {
                    const MDPMarketUpdate marketUpdate{snapshotSize++, *order};
                    _logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), marketUpdate.toString());
                    _snapshotSocket.send(&marketUpdate, sizeof(MDPMarketUpdate));
                    _snapshotSocket.sendAndRecv();
                }
            }
        }

        // send message designating the end of snapshot message
        const MDPMarketUpdate endMarketUpdate{snapshotSize++, {MarketUpdateType::SNAPSHOT_END, _lastIncSeqNum}};
        _logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), endMarketUpdate.toString());
        _snapshotSocket.send(&endMarketUpdate, sizeof(MDPMarketUpdate));
        _snapshotSocket.sendAndRecv();
    
        _logger.log("%:% %() % Published snapshot of % orders.\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), snapshotSize - 1);
    }

    auto SnapshotSynthesizer::run() noexcept -> void {
        _logger.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr));

        while (_run)
        {
            for (auto marketUpdate = _snapshotMdUpdates->getNextRead(); _snapshotMdUpdates->size() && marketUpdate; marketUpdate = _snapshotMdUpdates->getNextRead()) {
                _logger.log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), marketUpdate->toString().c_str());
                addToSnapshot(marketUpdate);
                _snapshotMdUpdates->updateReadIndex();
            }

            // send snapshot every minute
            if (getCurrentNanos() - _lastSnapshotTime > 60 * NANOS_TO_SEC) {
                publishSnapshot();
                _lastSnapshotTime = getCurrentNanos();
            }
        }
        
    }

}