#pragma once

#include <array>
#include <sstream>
#include "types.h"

using namespace Common;

namespace Trading {
    // enum representing state of an order sent by the OrderManager
    enum class OMOrderState : int8_t {
        INVALID = 0,
        PENDING_NEW = 1, // order was sent but didnt receive confirmation yet
        LIVE = 2, // order is live
        PENDING_CANCEL = 3, // cancellation of order was sent but didnt receive confirmation yet
        DEAD = 4
    };

    inline auto OMOrderStateToString(OMOrderState state) noexcept -> std::string {
        switch (state)
        {
        case OMOrderState::INVALID:
            return "INVALID";
        case OMOrderState::PENDING_NEW:
            return "PENDING_NEW";
        case OMOrderState::LIVE:
            return "LIVE";
        case OMOrderState::PENDING_CANCEL:
            return "PENDING_CANCEL";
        case OMOrderState::DEAD:
            return "DEAD";
        }

        return "UNKNOWN";
    };

    // struct representing an order
    struct OMOrder {
        TickerId tickerId = TickerId_INVALID;
        OrderId orderId = OrderId_INVALID;
        Side side = Side::Invalid;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        OMOrderState orderState = OMOrderState::INVALID;

        auto toString() const noexcept -> std::string {
            std::stringstream ss;
            ss << "OMOrder" << "["
            << "tid:" << tickerIdToString(tickerId) << " "
            << "oid:" << orderIdToString(orderId) << " "
            << "side:" << sideToString(side) << " "
            << "price:" << priceToString(price) << " "
            << "qty:" << qtyToString(qty) << " "
            << "state:" << OMOrderStateToString(orderState)
            << "]";
        }
    };

    // array that holds the max ammount of sides in an order
    typedef std::array<OMOrder, sideToIndex(Side::Max) + 1> OMOrderSideHashMap;
    // array that holds the max ammount of side in an order for every ticker
    typedef std::array<OMOrderSideHashMap, ME_MAX_TICKERS> OMOrderTickerSideHashMap;
}