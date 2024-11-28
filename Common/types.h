#pragma once

#include <macros.h>
#include <limits>
#include <cstdint>
#include <sstream>
#include <array>

namespace Common {
    // limits used in our system
    constexpr size_t LOG_QUEUE_SIZE = 8 * 1024 * 1024; // size of logger
    constexpr size_t ME_MAX_TICKERS = 8; // # of trading instruments supported by exchange
    // max number of unprocessed requests from all clients that matching engine has not processed yet
    // && also represents the max number of responses that order server has not published yet
    constexpr size_t ME_MAX_CLIENT_UPDATES = 256 * 1024; 
    // max number of market updates that have not been published by market data publisher
    constexpr size_t ME_MAX_MARKET_UPDATES = 256 * 1024;
    // max number of simultaneous market participants
    constexpr size_t ME_MAX_CLIENTS = 256;
    // max number of orders for specific trading instrument
    constexpr size_t ME_MAX_ORDER_IDS = 1024 * 1024;
    // max price depth of price levels maintained by matching engine
    constexpr size_t ME_MAX_PRICE_LEVELS = 256;

    // basic types used
    typedef uint64_t OrderId;
    typedef uint32_t TickerId;
    typedef uint32_t ClientId;
    typedef int64_t Price;
    typedef uint32_t Qty;
    typedef uint64_t Priority;

    
    constexpr auto OrderId_INVALID = std::numeric_limits<OrderId>::max();
    inline auto orderIdToString(OrderId order_id) noexcept -> std::string {
        if (UNLIKELY(OrderId_INVALID == order_id))
            return "INVALID";
        return std::to_string(order_id);
    }

    constexpr auto TickerId_INVALID = std::numeric_limits<TickerId>::max();
    inline auto tickerIdToString(TickerId ticker_id) noexcept -> std::string {
        if (UNLIKELY(TickerId_INVALID == ticker_id))
            return "INVALID";
        return std::to_string(ticker_id);
    }

    constexpr auto ClientId_INVALID = std::numeric_limits<ClientId>::max();
    inline auto clientIdToString(ClientId client_id) noexcept -> std::string {
        if (UNLIKELY(ClientId_INVALID == client_id))
            return "INVALID";
        return std::to_string(client_id);
    }

    constexpr auto Price_INVALID = std::numeric_limits<Price>::max();
    inline auto priceToString(Price price) noexcept -> std::string {
        if (UNLIKELY(Price_INVALID == price))
            return "INVALID";
        return std::to_string(price);
    }

    constexpr auto Qty_INVALID = std::numeric_limits<Qty>::max();
    inline auto qtyToString(Qty qty) noexcept -> std::string {
        if (UNLIKELY(Qty_INVALID == qty))
            return "INVALID";
        return std::to_string(qty);
    }

    constexpr auto Priority_INVALID = std::numeric_limits<Priority>::max();
    inline auto priorityToString(Priority priority) noexcept -> std::string {
        if (UNLIKELY(Priority_INVALID == priority))
            return "INVALID";
        return std::to_string(priority);
    }

    enum class Side {
        Invalid = 0,
        Buy = 1,
        Sell = -1,
        Max = 2,
    };

    inline auto sideToString(Side side) noexcept -> std::string {
        switch (side)
        {
        case Side::Buy:
            return "BUY";
        case Side::Sell:
            return "SELL";        
        case Side::Invalid:
            return "INVALID";
        default:
            return "UNKNOWN";        
        }
    }

    inline constexpr auto sideToIndex(Side side) noexcept -> size_t {
        return static_cast<size_t>(side) + 1;
    }

    inline constexpr auto sideToValue(Side side) noexcept -> int {
        return static_cast<int>(side);
    }

    // struct representing configuration provided to RiskManager
    struct RiskCfg {
        Qty maxOrderSize = 0; // maximum order size allowed by the strategy
        Qty maxPositions = 0; // maximum positions that strategy is allowed to build
        double maxLoss = 0;   // maximum loss that strategy is allowed to make before shutting it down 
    
        auto toString() const noexcept -> std::string {
            std::stringstream ss;
            ss << "RiskCfg{"
            << "max-order-size:" <<
            qtyToString(maxOrderSize) << " "
            << "max-position:" << qtyToString(maxPositions)
            << " "
            << "max-loss:" << maxLoss
            << "}";
            return ss.str();
        }
    }; 

    // struct representing configuration provided to TradingEngine for individual Ticker
    struct TradingEngineCfg {
        Qty clip = 0;
        double threshold = 0;
        RiskCfg riskCfg;

        auto toString() const noexcept -> std::string {
            std::stringstream ss;
            ss << "TradingEngineCfg{"
            << "clip:" << qtyToString(clip) << " "
            << "thresh:" << threshold << " "
            << "risk:" << riskCfg.toString()
            << "}";
        }
    };

    // array that holds configuration for all possible tickers
    typedef std::array<TradingEngineCfg, ME_MAX_TICKERS> TradingEngineCfgHashMap;
}