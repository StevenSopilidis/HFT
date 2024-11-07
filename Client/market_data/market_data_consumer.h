#pragma once

#include <functional>
#include <map>
#include "thread_utils.h"
#include "lf_queue.h"
#include "macros.h"
#include "mcast_socket.h"
#include "market_update.h"


namespace Trading {
    class MarketDataConsumer {
    public:
        MarketDataConsumer(Common::ClientId clientId, Exchange::MEMarketUpdateLFQueue *marketUpdates, const std::string &iface,
            const std::string &snapshopIp, int snapshotPort,
            const std::string &incrementalIp, int incrementalPort);
        ~MarketDataConsumer();

        MarketDataConsumer() = delete;
        MarketDataConsumer(const MarketDataConsumer &) = delete;
        MarketDataConsumer(const MarketDataConsumer &&) = delete;
        MarketDataConsumer &operator=(const MarketDataConsumer &) = delete;
        MarketDataConsumer &operator=(const MarketDataConsumer &&) = delete;


        auto start() noexcept -> void;
    private:
        typedef std::map<size_t, Exchange::MEMarketUpdate> QueuedMarketUpdates;
    private:
        // method for processing incoming requests
        auto recvCallback(McastSocket *socket) noexcept -> void;    

        auto run() noexcept -> void;
        auto stop() noexcept -> void;
        // method that starts synchronization process when packet drops is detected
        auto startSnapshotSync() noexcept -> void;
        // method for handling messages while on synchronization process
        auto queueMessage(bool isSnapshot, const Exchange::MDPMarketUpdate *request) -> void;
        // method for checking if recovery is possible from queued up messages
        auto checkSnapshotSync() noexcept -> void;

        // next expected sequence number used for sync with exchange
        size_t _nextExpIncSeqNum = 1;
        // queue for publishing market updates to trading engine
        Exchange::MEMarketUpdateLFQueue* _incomingMDUpdates = nullptr;
        volatile bool _run;
        std::string _timeStr;
        Logger _logger;
        // socket for receiving incremental updates from exchange
        Common::McastSocket _incrementalMcastSocket;
        // socket for receiving ordebook snapshots
        Common::McastSocket _snapshotMcastSocket;
        // indicates wether or not a packet has dropped
        bool _inRecovery;
        const std::string _iface; 
        // for connecting to snapshot stream
        const std::string _snapshotIp;
        const int _snapshotPort;
        // queue for holding snapshot messages & incremental market updates 
        QueuedMarketUpdates _snapshotQueuedMsgs, _incrementalQueuedMsgs;
    };
}