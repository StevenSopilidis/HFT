#pragma once

#include <array>
#include <sstream>
#include "types.h"

using namespace Common;

namespace Trading {
    // struct that represents single market order
    struct MarketOrder {
        OrderId orderId = OrderId_INVALID;
        Side side = Side::Invalid;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        Priority priority = Priority_INVALID;
        MarketOrder* prevOrder = nullptr;
        MarketOrder* nextOrder = nullptr;

        MarketOrder() = default;
        MarketOrder(OrderId orderId, Side side, Price price, Qty qty, Priority priority, MarketOrder* prevOrder, MarketOrder* nextOrder) :
            orderId(orderId), side(side), price(price), qty(qty), priority(priority), prevOrder(prevOrder), nextOrder(nextOrder)
        {}

        auto toString() const noexcept -> std::string {
            std::stringstream ss;
            ss << "MarketOrder" << "["
            << "oid:" << orderIdToString(orderId) << " "
            << "side:" << sideToString(side) << " "
            << "price:" << priceToString(price) << " "
            << "qty:" << qtyToString(qty) << " "
            << "prio:" << priorityToString(priority) << " "
            << "prev:" << orderIdToString(prevOrder ? prevOrder->orderId : OrderId_INVALID) << " "
            << "next:" << orderIdToString(nextOrder ? nextOrder->orderId : OrderId_INVALID) << "]";
            return ss.str();
        };
    };

    // map that maps ids to MarketOrders
    typedef std::array<MarketOrder *, ME_MAX_ORDER_IDS> OrderHashMap;

    // struct that represents grouping of orders of same price level
    struct MarketOrderAtPrice {
        Side side = Side::Invalid;
        Price price = Price_INVALID;
        MarketOrder* firstMarketOrder = nullptr;
        MarketOrderAtPrice* prevEntry = nullptr;
        MarketOrderAtPrice* nextEntry = nullptr;

        MarketOrderAtPrice() = default;
        MarketOrderAtPrice(Side side, Price price, MarketOrder* firstMarketOrder, MarketOrderAtPrice* prevEntry, MarketOrderAtPrice* nextEntry) :
        side(side), price(price), firstMarketOrder(firstMarketOrder), prevEntry(prevEntry), nextEntry(nextEntry)
        {}

        auto toString() const noexcept -> std::string {
            std::stringstream ss;
            ss << "MarketOrdersAtPrice["
            << "side:" << sideToString(side) << " "
            << "price:" << priceToString(price) << " "
            << "first_mkt_order:" << (firstMarketOrder ? firstMarketOrder->toString() : "null") << " "
            << "prev:" << priceToString(prevEntry ? prevEntry->price : Price_INVALID) << " "
            << "next:" << priceToString(nextEntry ? nextEntry->price : Price_INVALID) << "]";
            return ss.str();
        }
    };

    // map that maps price levels to MarketOrderAtPrice
    typedef std::array<MarketOrderAtPrice*, ME_MAX_PRICE_LEVELS> OrdersAtPriceHashMap;
 
    // struct representing a BBO (Best Bid & offer)
    struct BBO {
        // most aggressive bid and ask prices
        Price bidPrice = Price_INVALID;
        Price askPrice = Price_INVALID;
        // total ammount of bid & ask qty
        Qty bidQty = Qty_INVALID;
        Qty askQty = Qty_INVALID;

        auto toString() const {
            std::stringstream ss;
            ss << "BBO{"
            << qtyToString(bidQty) << "@" << priceToString(bidPrice)
            << "X"
            << priceToString(askPrice) << "@" << qtyToString(askQty) << "}";
            return ss.str();
        };

    };
}   