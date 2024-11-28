#pragma once

#include "macros.h"
#include "logging.h"
#include "client_request.h"
#include "client_response.h"
#include "om_order.h"
#include "risk_manager.h"

using namespace Common;

namespace Trading {
    class TradingEngine;

    class OrderManager {
    public:
        OrderManager(Logger* logger, TradingEngine* tradingEngine, RiskManager& riskManager);
        OrderManager() = delete;
        OrderManager(const OrderManager &) = delete;
        OrderManager(const OrderManager &&) = delete;
        OrderManager &operator=(const OrderManager &) = delete;
        OrderManager &operator=(const OrderManager &&) = delete;

        auto getOMOrderSideHashMap(TickerId tickerId) const noexcept {
            return &{_tickerSideOrder.at(tickerId)};
        }

        auto moveOrders(TickerId tickerId, Price bidPrice, Price askPrice, Qty clip) noexcept -> void;
        auto onOrderUpdate(const Exchange::MEClientResponse* clientResponse) noexcept -> void;
    private:
        auto moveOrder(OMOrder* order, TickerId tickerId, Price price, Side side, Qty qty) noexcept -> void;
        auto newOrder(OMOrder* order, TickerId tickerId, Price price, Side side, Qty qty) noexcept -> void;
        auto cancelOrder(OMOrder* order) noexcept -> void;
        
        
        TradingEngine* _tradingEngine = nulltpr;
        const RiskManager& _riskManager;
        std::string _timeStr;
        Logger* _logger;
        OMOrderTickerSideHashMap _tickerSideOrder;
        OrderId _nextOrderId = 1;
    };
}