#pragma once

#include "types.h"
#include "position_keeper.h"
#include "om_order.h"

namespace Trading {
    class OrderManager;

    // enum that represents all possible outcomes of RiskCheck
    enum class RiskCheckResult : int8_t {
        INVALID = 0,
        ORDER_TOO_LARGE = 1,
        POSITION_TOO_LARGE = 2,
        LOSS_TOO_LARGE = 3,
        ALLOWED = 4
    };

    inline auto riskCheckResultToString(RiskCheckResult result) noexcept -> std::string{
        switch (result)
        {
        case RiskCheckResult::INVALID :
            return "INVALID";
        case RiskCheckResult::ORDER_TOO_LARGE:
            return "ORDER_TOO_LARGE";
        case RiskCheckResult::POSITION_TOO_LARGE:
            return "POSITION_TOO_LARGE";
        case RiskCheckResult::LOSS_TOO_LARGE:
            return "LOSS_TOO_LARGE";       
        case RiskCheckResult::ALLOWED:
            return "ALLOWED";       
        }
    }

    // struct that is holds information about performing risk checks for single trading instrument
    struct RiskInfo {
        const PositionInfo* positionInfo = nullptr;
        RiskCfg riskCfg;

        auto checkPreTradeRisk(Side side, Qty qty) const noexcept -> RiskCheckResult {
            if (UNLIKELY(qty > riskCfg.maxOrderSize))
                return RiskCheckResult::ORDER_TOO_LARGE;
            
            if (UNLIKELY(std::abs(positionInfo->position + sideToValue(side) * static_cast<int32_t>(qty)) > static_cast<int32_t>(riskCfg.maxPositions)))
                return RiskCheckResult::POSITION_TOO_LARGE;

            if (UNLIKELY(positionInfo->totalPnL < riskCfg.maxLoss))
                return RiskCheckResult::LOSS_TOO_LARGE;

            return RiskCheckResult::ALLOWED;
        }

        auto toString() const noexcept -> std::string {
            std::stringstream ss;
            ss << "RiskInfo" << "["
            << "pos:" << positionInfo->toString() << " "
            << riskCfg.toString()
            << "]";
            return ss.str();
        }
    };

    // array that holds info for performing risk checks for all available trading instruments
    typedef std::array<RiskInfo, ME_MAX_TICKERS> TickerRiskInfoHashMap;


    class RiskManager {
    public:
        RiskManager(Logger* logger, const PositionKeeper* positionKeeper, const TradingEngineCfgHashMap& tickerCfg) : _logger(logger) {
            for(TickerId i = 0; i < ME_MAX_TICKERS; i++) {
                _tickerRisk.at(i).positionInfo = positionKeeper->getPositionInfo(i);
                _tickerRisk.at(i).riskCfg = tickerCfg[i].riskCfg;
            }
        }

        auto checkPreTradeRisk(TickerId tickerId, Side side, Qty qty) const noexcept ->  RiskCheckResult {
            return _tickerRisk.at(tickerId).checkPreTradeRisk(side, qty);
        }

        RiskManager() = delete;
        RiskManager(const RiskManager &) = delete;
        RiskManager(const RiskManager &&) = delete;
        RiskManager &operator=(const RiskManager &) = delete;
        RiskManager &operator=(const RiskManager &&) = delete;
    
    private:
        std::string _timeStr;
        Logger* _logger;
        TickerRiskInfoHashMap _tickerRisk;
    };
};