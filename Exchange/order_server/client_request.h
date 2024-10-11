#pragma once

#include <sstream>
#include "types.h"
#include "types.h"
#include "lf_queue.h"

using namespace Common;

namespace Exchange {
#pragma pack(push, 1)
    enum class ClientRequestType : uint8_t {
        INVALID = 0,
        NEW = 1,
        CANCEL = 2,
    };

    inline std::string clientRequestTypeToString(ClientRequestType type) noexcept {
        switch (type)
        {
        case ClientRequestType::INVALID:
            return "INVALID";
        case ClientRequestType::NEW:
            return "NEW";
        case ClientRequestType::CANCEL:
            return "CANCEL";
        }

        return "UNKNOWN";
    }

    // struct that represents order request from client, its the internal 
    // representation used by the matching engine, its now what the client 
    // neccesserily uses
    struct MEClientRequest {
        ClientRequestType type = ClientRequestType::INVALID;
        ClientId clientId = ClientId_INVALID;
        TickerId tickerId = TickerId_INVALID;
        OrderId orderId = OrderId_INVALID;
        Side side = Side::Invalid;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        auto toString() const {
            std::stringstream ss;
            ss << "MEClientRequest"
            << " ["
            << "type:" << clientRequestTypeToString(type)
            << " client:" << clientIdToString(clientId)
            << " ticker:" << tickerIdToString(tickerId)
            << " oid:" << orderIdToString(orderId)
            << " side:" << sideToString(side)
            << " qty:" << qtyToString(qty)
            << " price:" << priceToString(price)
            << "]";
            return ss.str();
        }
    };

#pragma pack(pop)

    // queue that will be used for communication between order gateway ---> matching engine  
    typedef LFQueue<MEClientRequest> ClientRequestLFQueue;
}