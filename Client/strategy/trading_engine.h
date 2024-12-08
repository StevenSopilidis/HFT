#pragma once

#include <functional>
#include "thread_utils.h"
#include "time_utils.h"
#include "macros.h"
#include "logging.h"
#include "client_request.h"
#include "client_response.h"
#include "market_update.h"
#include "market_order_book.h"
#include "feature_engine.h"
#include "position_keeper.h"
#include "order_manager.h"
#include "risk_manager.h"
#include "market_maker.h"
#include "liquidity_taker.h"

namespace Trading {
    class TradingEngine {
    public:
        TradingEngine(ClientId clientId, AlgoType algoType, const TradingEngineCfgHashMap& tickerCfg, Exchange::ClientRequestLFQueue* clientRequests,
        Exchange::ClientResponseLFQueue* clientResponse, Exchange::MEMarketUpdateLFQueue* marketUpdates);

        ~TradingEngine();

        TradingEngine() = delete;
        TradingEngine(const TradingEngine &) = delete;
        TradingEngine(const TradingEngine &&) = delete;
        TradingEngine &operator=(const TradingEngine &) = delete;
        TradingEngine &operator=(const TradingEngine &&) = delete;

        auto start() noexcept -> void;
        auto stop() noexcept -> void;

        // wrapper for function for dispatching exchange events
        std::function<void(TickerId tickerId, Price price, Side side, MarketOrderBook *book)> algoOnOrderBookUpdate;
        std::function<void(const Exchange::MEMarketUpdate *marketUpdate, MarketOrderBook *book)> algoOnTradeUpdate;
        std::function<void(const Exchange::MEClientResponse *clientResponse)> algoOnOrderUpdate;

        auto sendClientRequest(const Exchange::MEClientRequest* clientRequest) noexcept -> void;
        auto onOrderBookUpdate(TickerId tickerId, Price price, Side side, MarketOrderBook* book) noexcept -> void;
        auto onOrderUpdate(const Exchange::MEClientResponse *response) noexcept -> void;
        auto onTradeUpdate(const Exchange::MEMarketUpdate *market_update, MarketOrderBook *book) noexcept -> void;

        auto initLastEventTime() noexcept -> void;
        auto silenceSeconds() const noexcept -> int64_t;
        auto getClientId() const noexcept -> ClientId;
    private:
        auto run() noexcept -> void;

        const ClientId _clientId;
        
        MarketOrderBookHashMap _tickerOrderBook;
        
        Exchange::ClientRequestLFQueue* _outgoingOgwRequests = nullptr; // channel to send requests to order gateway
        Exchange::ClientResponseLFQueue* _incomingOgwResponses = nullptr; // channel to receive responses from order gateway
        Exchange::MEMarketUpdateLFQueue* _incomingMdUpdates = nullptr; // channel to receive market updates from market data consumer
        
        Nanos _lastEventTime = 0; // keeps track of time when we last received message from exchange
        
        volatile bool _run = false;
        std::string _timeStr;
        Logger _logger;

        FeatureEngine _featureEngine;
        PositionKeeper _positionKeeper;
        OrderManager _orderManager;
        RiskManager _riskManager;

        // trading algorithms, only one will be instantiated at each trading engine instance
        MarketMaker* _mmAlgo = nullptr;
        LiquidityTaker* _takerAlgo = nulltpr;

        // default methods for initializing functinos for exchange events
        auto defaultAlgoOnOrdeBookUpdate(TickerId tickerId, Price price, Side side, MarketOrderBook *book) noexcept -> void {
            _logger.log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), tickerId, priceToString(price).c_str(), sideToString(side).c_str());
        }

        auto defaultAlgoOnTradeUpdate(const Exchange::MEMarketUpdate *marketUpdate, MarketOrderBook *book) noexcept -> void {
            _logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), marketUpdate->toString().c_str());
        }

        auto defaultAlgoOnOrderUpdate(const Exchange::MEClientResponse *clientResponse) noexcept -> void {
            _logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), clientResponse->toString().c_str());
        }
    };
}