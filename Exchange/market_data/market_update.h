#pragma once

#include <sstream>
#include "types.h"
#include "lf_queue.h"

using namespace Common;

namespace Exchange {
#pragma pack(push, 1)
    enum class MarketUpdateType : uint8_t {
        INVALID = 0,
        CLEAR = 1, // instructs market participants to clear their order book
        ADD = 2,
        MODIFY = 3,
        CANCEL = 4,
        TRADE = 5, 
        SNAPSHOT_START = 6, // signifies that a snapshot message is starting
        SNAPSHOT_END = 7,  // signifies that a snapshot update has been delivered
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

    // struct that represents market update that will be sent by the market data publisher
    // to the market participant
    struct MDPMarketUpdate {
        // will be used by the client to identify if any packet was dropped
        size_t seqNumber = 0;
        MEMarketUpdate meMarketUpdate;

        auto toString() const {
            std::stringstream ss;
            ss << "MDPMarketUpdate"
            << " ["
            << " seq:" << seqNumber
            << " " << meMarketUpdate.toString()
            << "]";
            return ss.str();
        }
    };
    

#pragma pack(pop)

    // queue used for communicatoin from matching engine to market data publisher
    typedef LFQueue<MEMarketUpdate> MEMarketUpdateLFQueue;
    typedef LFQueue<MDPMarketUpdate> MDPMarketUpdateLFQueue;
}