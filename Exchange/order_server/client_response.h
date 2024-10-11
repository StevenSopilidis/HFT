#pragma once

#include <sstream>
#include "types.h"
#include "types.h"
#include "lf_queue.h"

using namespace Common;

namespace Exchange {
#pragma pack(push, 1)
    enum class ClientResponseType : uint8_t {
        INVALID = 0,
        ACCEPTED = 1,
        CANCELED = 2,
        FILLED = 3,
        CANCEL_REJECTED = 4,
    };

    inline std::string clientResponseTypeToString(ClientResponseType type) {
        switch (type) {
            case ClientResponseType::ACCEPTED:
            return "ACCEPTED";
            case ClientResponseType::CANCELED:
            return "CANCELED";
            case ClientResponseType::FILLED:
            return "FILLED";
            case ClientResponseType::CANCEL_REJECTED:
            return "CANCEL_REJECTED";
            case ClientResponseType::INVALID:
            return "INVALID";
        }
        
        return "UNKNOWN";
    };

    struct MEClientResponse {
        ClientResponseType type = ClientResponseType::INVALID;
        ClientId clientId = ClientId_INVALID;
        TickerId tickerId = TickerId_INVALID;
        OrderId clientOrderId = OrderId_INVALID;
        OrderId marketOrderId = OrderId_INVALID;
        Side side = Side::Invalid;
        Price price = Price_INVALID;
        Qty execQty = Qty_INVALID;
        Qty leavesQty = Qty_INVALID;

        auto toString() const {
            std::stringstream ss;
            ss << "MEClientResponse"
            << " ["
            << "type:" << clientResponseTypeToString(type)
            << " client:" << clientIdToString(clientId)
            << " ticker:" << tickerIdToString(tickerId)
            << " coid:" << orderIdToString(clientOrderId)
            << " moid:" << orderIdToString(marketOrderId)
            << " side:" << sideToString(side)
            << " exec_qty:" << qtyToString(execQty)
            << " leaves_qty:" << qtyToString(leavesQty)
            << " price:" << priceToString(price)
            << "]";
            return ss.str();
        }
    };

#pragma pack(pop)
    
    // queue that will be used for communication between order matching engine ---> order gateway  
    typedef LFQueue<MEClientResponse> ClientResponseLFQueue;

}