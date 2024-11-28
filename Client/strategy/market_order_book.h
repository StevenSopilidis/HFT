#pragma once

#include "types.h"
#include "mem_pool.h"
#include "logging.h"
#include "market_order.h"
#include "market_update.h"

namespace Trading {
    class TradingEngine;
    
    class MarketOrderBook {
    public:
        MarketOrderBook(TickerId tickerId, Logger* logger);
        ~MarketOrderBook();

        MarketOrderBook() = delete;
        MarketOrderBook(const MarketOrderBook& ) = delete;
        MarketOrderBook(const MarketOrderBook&& ) = delete;
        MarketOrderBook& operator=(const MarketOrderBook&) = delete;
        MarketOrderBook& operator=(const MarketOrderBook&&) = delete;

        // method for setting the trading engine
        auto setTradingEngine(TradingEngine* tradingEngine) noexcept -> void;
        // method to be called when we receive market update data
        auto onMarketUpdate(const Exchange::MEMarketUpdate* marketUpdate) noexcept -> void;
        // method for getting BBO
        auto getBBO() const noexcept -> const BBO*;

    private:
        // method for updating BBO structure
        auto updateBBO(bool updateBid, bool updateAsks) noexcept;
        auto priceToIndex(Price price) const noexcept -> void;
        auto getOrdersAtPrice(Price price) const noexcept;
        auto addOrder(MarketOrder* order) noexcept -> void;
        auto addOrdersAtPrice(MarketOrderAtPrice* newOrdersAtPrice) noexcept -> void;
        auto removeOrdersAtPrice(Side side, Price price) noexcept -> void;
        auto removeOrder(MarketOrder* order) noexcept -> void; 
    
        const TickerId _tickerId;
        TradingEngine* _tradingEngine = nullptr;
        // hashmap to map orderId to order object
        OrderHashMap _oidToOrder;
        MemPool<MarketOrderAtPrice> _ordersAtPricePool;
        MemPool<MarketOrder> _orderPool;
        MarketOrderAtPrice* _asksByPrice;
        MarketOrderAtPrice* _bidsByPrice;
        OrdersAtPriceHashMap _priceOrdersAtPrice;
        BBO _bbo;
        Logger* _logger;
        std::string _timeStr;
    };

    typedef std::array<MarketOrderBook*, ME_MAX_TICKERS> MarketOrderBookHashMap;
};