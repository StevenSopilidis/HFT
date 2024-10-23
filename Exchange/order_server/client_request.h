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

    // struct that represents client order request comming from order gateway
    // to matching engine
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

    // struct that represents message sent by market participant to order gateway
    struct OMClientRequest {
        size_t seqNum; // for sync purposes
        MEClientRequest meClientRequest;

        auto toString() const {
            std::stringstream ss;
            ss << "OMClientRequest"
            << " ["
            << "seq:" << seqNum
            << " " << meClientRequest.toString()
            << "]";
            return ss.str();
        }
    };

#pragma pack(pop)

    // queue that will be used for communication between order gateway ---> matching engine  
    typedef LFQueue<MEClientRequest> ClientRequestLFQueue;
}