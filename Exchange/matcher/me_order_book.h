#pragma once

#include "types.h"
#include "mem_pool.h"
#include "logging.h"
#include "client_response.h"
#include "market_update.h"
#include "me_order.h"

using namespace Common;

namespace Exchange {
    class MatchingEngine;

    class MEOrderBook final {
    public:
        MEOrderBook(TickerId tickerId, Logger* logger, MatchingEngine* matchingEngine);
        ~MEOrderBook();
        MEOrderBook() = delete;
        MEOrderBook(const MEOrderBook &) = delete;
        MEOrderBook(const MEOrderBook &&) = delete;
        MEOrderBook &operator=(const MEOrderBook &) = delete;
        MEOrderBook &operator=(const MEOrderBook &&) = delete;

        // method for creating new order
        auto add(ClientId clientId, OrderId clientOrderId, TickerId tickerId, Side side, Price price, Qty qty) noexcept -> void;
        // method for cancelling order
        auto cancel(ClientId clientId, OrderId orderId, TickerId tickerId) noexcept -> void;
    private:
        auto generateNewMarketOrderId() noexcept -> OrderId;
        auto priceToIndex(Price price) const noexcept -> int;
        auto toString(bool detailed, bool validity_check) const noexcept -> std::string;
        auto getOrdersAtPrice(Price price) const noexcept;
        // function for getting priority of order
        auto getNextPriority(Price price) const noexcept -> uint64_t;
        // function for adding order to OrderBook
        auto addOrder(MEOrder* order) noexcept -> void;
        // function for removing order from OrderBook
        auto removeOrder(MEOrder* order) noexcept -> void;
        // function for adding new price level to OrderBook
        auto addOrdersAtPrice(MEOrderAtPrice* newOrdersAtPrice) noexcept -> void;
        // function for removing price level from OrderBook
        auto removeOrdersAtPrice(Side side, Price price) noexcept -> void;
        // function for matching and agreessive order up against orders in the OrderBook
        auto checkForMatch(ClientId clientId, OrderId clientOrderId, TickerId tickerId, Side side, Price price, Qty qty, OrderId newMarketOrderId) noexcept -> Qty;
        // function for matching aggressive order up against iterator provided and sends client response and market updates
        auto match(TickerId tickerId, ClientId clientId, Side side, OrderId clientOrderId, OrderId newMarketOrderId, MEOrder* itr, Qty* leavesQty) noexcept -> void;

        TickerId _tickerId = TickerId_INVALID;
        MatchingEngine* _matchingEngine = nullptr;
        ClientOrderHashMap _cidOidToOrder;
        MEOrderAtPrice* _bidsByPrice; // tracks bids
        MEOrderAtPrice* _asksByPrice; // tracks asks
        OrdersAtPriceHashMap _priceOrdersAtPrice; // array that holds orders of different prices
        MemPool<MEOrder> _orderPool;
        MemPool<MEOrderAtPrice> _ordersAtPricePool;
        MEClientResponse _clientResponse;
        MEMarketUpdate _marketUpdate;
        OrderId _nextMarketOrderId = 1;
        std::string _timeStr;
        Logger* _logger = nullptr;
    };

    // collection of order books for different trading instruments
    typedef std::array<MEOrderBook *, ME_MAX_TICKERS> OrderBookHashMap;
}