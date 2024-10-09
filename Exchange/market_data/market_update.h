#pragma once

#include <sstream>
#include "Common/include/types.h"
#include "Common/include/lf_queue.h"

using namespace Common;

namespace Exchange {
#pragma pack(push, 1)
    enum class MarketUpdateType : uint8_t {
        INVALID = 0,
        ADD = 1,
        MODIFY = 2,
        CANCEL = 3,
        TRADE = 4
    };

    inline std::string marketUpdateTypeToString(MarketUpdateType type) noexcept {
        switch (type) {
            case MarketUpdateType::ADD:
                return "ADD";
            case MarketUpdateType::MODIFY:
                return "MODIFY";
            case MarketUpdateType::CANCEL:
                return "CANCEL";
            case MarketUpdateType::TRADE:
                return "TRADE";
            case MarketUpdateType::INVALID:
                return "INVALID";
        }

        return "UNKOWN";
    };

    // struct representing market update sent by matching engine to market data publisher
    struct MEMarketUpdate {
        MarketUpdateType type = MarketUpdateType::INVALID;
        OrderId orderId = OrderId_INVALID;
        TickerId tickerId = TickerId_INVALID;
        Side side = Side::Invalid;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        Priority priority = Priority_INVALID;

        auto toString() const {
            std::stringstream ss;
            ss << "MEMarketUpdate"
            << " ["
            << " type:" << marketUpdateTypeToString(type)
            << " ticker:" << tickerIdToString(tickerId)
            << " oid:" << orderIdToString(orderId)
            << " side:" << sideToString(side)
            << " qty:" << qtyToString(qty)
            << " price:" << priceToString(price)
            << " priority:" << priorityToString(priority)
            << "]";
            return ss.str();
        };
    };

#pragma pack(pop)

    // queue used for communicatoin from matching engine to market data publisher
    typedef LFQueue<MEMarketUpdate> MarketUpdateLFQueue;
}