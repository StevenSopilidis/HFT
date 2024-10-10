#pragma once

#include <array>
#include <sstream>
#include <Common/include/types.h>

using namespace Common;

namespace Exchange {
    // struct that represents and Order inside the Matching engine
    struct MEOrder {
        TickerId tickerId = TickerId_INVALID;
        ClientId clientId = ClientId_INVALID;
        OrderId clientOrderId = OrderId_INVALID;
        OrderId marketOrderId = OrderId_INVALID;
        Side side = Side::Invalid;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        Priority priority = Priority_INVALID;
        // pointers used for implementing the double linked list
        // where each points to MEOrder of same price
        MEOrder* nextOrder = nullptr;
        MEOrder* prevOrder = nullptr;

        MEOrder() = default;
        MEOrder(TickerId ticker_id, ClientId client_id, OrderId client_order_id, OrderId market_order_id, Side side, Price price, Qty qty, Priority priority, MEOrder *prev_order, MEOrder *next_order) noexcept
        : tickerId(ticker_id), clientId(client_id), clientOrderId(client_order_id), marketOrderId(market_order_id), side(side),
            price(price), qty(qty), priority(priority), prevOrder(prev_order), nextOrder(next_order) {}

        auto toString() noexcept -> std::string {
            std::stringstream ss;
            ss << "MEOrder" << "["
            << "ticker:" << tickerIdToString(tickerId) << " "
            << "cid:" << clientIdToString(clientId) << " "
            << "oid:" << orderIdToString(clientOrderId) << " "
            << "moid:" << orderIdToString(marketOrderId) << " "
            << "side:" << sideToString(side) << " "
            << "price:" << priceToString(price) << " "
            << "qty:" << qtyToString(qty) << " "
            << "prio:" << priorityToString(priority) << " "
            << "prev:" << orderIdToString(prevOrder ? prevOrder->marketOrderId : OrderId_INVALID) << " "
            << "next:" << orderIdToString(nextOrder ? nextOrder->marketOrderId : OrderId_INVALID) << "]";
            return ss.str();
        }
    };


    // struct that encapsualtes a list of MEOrders of the same price
    struct MEOrderAtPrice {
        Side side = Side::Invalid;
        Price price = Price_INVALID;
        MEOrder* firstMeOrder = nullptr;
        MEOrderAtPrice* nextEntry = nullptr;
        MEOrderAtPrice* prevEntry = nullptr;

        MEOrderAtPrice() = default;
        MEOrderAtPrice(Side side, Price price, MEOrder* firstMeOrder, MEOrderAtPrice* nextEntry, MEOrderAtPrice* prevEntry) : 
        side(side), price(price), firstMeOrder(firstMeOrder), nextEntry(nextEntry), prevEntry(prevEntry) {}
    
        auto toString() noexcept -> std::string {
            std::stringstream ss;
            ss << "MEOrdersAtPrice["
            << "side:" << sideToString(side) << " "
            << "price:" << priceToString(price) << " "
            << "first_me_order:" << (firstMeOrder ? firstMeOrder->toString() : "null") << " "
            << "prev:" << priceToString(prevEntry ? prevEntry->price : Price_INVALID) << " "
            << "next:" << priceToString(nextEntry ? nextEntry->price : Price_INVALID) << "]";
            return ss.str();
        };
    };


    // hash map implemented via std::array that encapsulated all Orders
    typedef std::array<MEOrderAtPrice*, ME_MAX_PRICE_LEVELS> OrdersAtPriceHashMap;
}