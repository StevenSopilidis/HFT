#pragma once

#include <functional>
#include "types.h"
#include "snapshot_synthesizer.h"

namespace Exchange {
    class MarketDataPublisher {
    public:
        MarketDataPublisher(MEMarketUpdateLFQueue* marketUpdates, const std::string& iface, const std::string& snapshotIp, int snapshotPort, const std::string& incrementalIp, int incrementalPort);
        ~MarketDataPublisher();

        MarketDataPublisher() = delete;
        MarketDataPublisher(const MarketDataPublisher &) = delete;
        MarketDataPublisher(const MarketDataPublisher &&) = delete;
        MarketDataPublisher &operator=(const MarketDataPublisher &) = delete;
        MarketDataPublisher &operator=(const MarketDataPublisher &&) = delete;

        auto start() noexcept -> void;
        auto stop() noexcept -> void;
    private:
        auto run() noexcept -> void;
        // sequence num for market updates
        size_t _nextIncSeqNum = 1;
        // channel used to receive updates by matching engine
        MEMarketUpdateLFQueue* _outgoingMdUpdates = nullptr; 
        // channel to communicate with snapshot synthesizer
        MDPMarketUpdateLFQueue _snapshotMdUpdates;
        volatile bool _run = false;
        std::string _timeStr;
        Logger _logger;
        // socket for multicasting the market updates
        Common::McastSocket _incrementalSocket;
        SnapshotSynthesizer* _snapshotSynthesizer = nullptr;
    };
}