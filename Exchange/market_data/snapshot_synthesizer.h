#pragma once

#include <cstring>
#include "types.h"
#include "thread_utils.h"
#include "lf_queue.h"
#include "macros.h"
#include "mcast_socket.h"
#include "mem_pool.h"
#include "logging.h"
#include "market_update.h"
#include "me_order.h"

using namespace Common;

namespace Exchange {
    class SnapshotSynthesizer {
    public:
        SnapshotSynthesizer(MDPMarketUpdateLFQueue* marketUpdates, const std::string& iface, const std::string& snapshotIp, const int snapshotPort);
        ~SnapshotSynthesizer();

        SnapshotSynthesizer() = delete;
        SnapshotSynthesizer(const SnapshotSynthesizer &) = delete;
        SnapshotSynthesizer(const SnapshotSynthesizer &&) = delete;
        SnapshotSynthesizer &operator=(const SnapshotSynthesizer &) = delete;
        SnapshotSynthesizer &operator=(const SnapshotSynthesizer &&) = delete;

        auto start() noexcept -> void;
        // method for publishing the order book snapshot
        auto publishSnapshot() noexcept -> void;
        
        auto stop() noexcept -> void;

    private:
        auto run() noexcept -> void;
        // method for adding marketUpdate to snapshot
        auto addToSnapshot(const MDPMarketUpdate* marketUpdate);

        // queue that receives updates from MarketDataPublisher
        MDPMarketUpdateLFQueue* _snapshotMdUpdates = nullptr;
        Logger _logger;
        volatile bool _run = false;
        std::string _timeStr;
        McastSocket _snapshotSocket;
        // contains orders for each ticker
        std::array<std::array<MEMarketUpdate*, ME_MAX_ORDER_IDS>, ME_MAX_TICKERS> _tickerOrders;
        // seq num of last update received
        size_t _lastIncSeqNum = 0;
        // time of when last snapshot was sent
        Nanos _lastSnapshotTime = 0;
        MemPool<MEMarketUpdate> _orderPool;
    };
}