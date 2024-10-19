#pragma once

#include "lf_queue.h"
#include "thread_utils.h"
#include "macros.h"
#include "logging.h"
#include "client_request.h"
#include "client_response.h"
#include "market_update.h"

#include "me_order_book.h"

using namespace Common;

namespace Exchange {
    class MatchingEngine final {
        public:
            MatchingEngine(ClientRequestLFQueue* clientRequests, ClientResponseLFQueue* clientResponses, MEMarketUpdateLFQueue* marketUpdates);
            ~MatchingEngine();
            MatchingEngine() = delete;
            MatchingEngine(const MatchingEngine& ) = delete;
            MatchingEngine(const MatchingEngine&& ) = delete;
            MatchingEngine &operator=(const MatchingEngine& ) = delete;
            MatchingEngine &operator=(const MatchingEngine&& ) = delete;

            auto start() noexcept -> void;
            auto stop() noexcept -> void;

            auto sendClientResponse(const MEClientResponse* clientResponse) noexcept -> void;
            auto sendMarketUpdate(const MEMarketUpdate* marketUpdate) noexcept -> void;

        private:
            auto processClientRequest(const MEClientRequest* clientRequest) noexcept -> void;
            auto run() noexcept -> void;


            ClientRequestLFQueue* _incomingRequests = nullptr;
            // outgoing order gateway responses
            ClientResponseLFQueue* _outgoingOgwResponses = nullptr;
            // outgoing market data updates
            MEMarketUpdateLFQueue* _outgoingMDUpdates = nullptr;
            volatile bool _run = false;
            std::string _timeStr;
            Logger _logger;
            OrderBookHashMap _tickerOrderBook; 
    };

}
