#pragma once

#include "macros.h"
#include "logging.h"
#include "types.h"
#include "client_response.h"
#include "market_order_book.h"

using namespace Common;

namespace Trading {
    // struct that tracks positions and P&L for single Ticker
    struct PositionInfo {
        int32_t position = 0;  // current num of positions
        // reaPnL -> for positions that are closed. Realized PnL=(Sell Price−Buy Price)×Quantity Sold
        // unrealPnL -> for currently open positions. Unrealized PnL=(Market Price−Average Entry Price)×Quantity Held
        // totalPnL -> sum of 2 above.
        double realPnL = 0, unrealPnL = 0, totalPnL = 0;
        // array that keeps track of volume weighted average price on each side
        // for both long and short positions
        // The VWAP (Volume-Weighted Average Price) is a common metric in trading, used to measure the average 
        // price of a security over a given time period, weighted by volume. It provides insights into whether a 
        // trade was executed at a favorable price compared to the overall market trend.
        // caculated == SUM(price * volume) * SUM(volume)
        std::array<double, sideToIndex(Side::Max) + 1> openVWAP;
        // var to keep track of total qty that has been executed
        Qty volume = 0;
        const BBO* bbo = nullptr;

        auto toString() const {
            std::stringstream ss;
            ss << "Position{"
                << "pos:" << position
                << " u-pnl:" << unrealPnL
                << " r-pnl:" << realPnL
                << " t-pnl:" << totalPnL
                << " vol:" << qtyToString(volume)
                << " vwaps:[" << (position ? openVWAP.at(sideToIndex(Side::Buy)) / std::abs(position) : 0)
                << "X" << (position ? openVWAP.at(sideToIndex(Side::Sell)) / std::abs(position) : 0)
                << "] "
                << (bbo ? bbo->toString() : "") << "}";
            
            return ss.str();
        }

        // function to be called when a trade was executed
        auto addFill(const Exchange::MEClientResponse* clientResponse, Logger* logger) noexcept -> void {
            const auto oldPosition = position;
            const auto sideIndex = sideToIndex(clientResponse->side);
            const auto oppSideIndex = sideToIndex(clientResponse->side == Side::Buy? Side::Sell : Side::Buy);
            const auto sideValue = sideToValue(clientResponse->side);

            position += clientResponse->execQty * sideValue;
            volume += clientResponse->execQty;

            if (oldPosition * sideValue >= 0) { // opened position
                openVWAP[sideIndex] += clientResponse->price * clientResponse->execQty;
            } else { // closed position
                const auto oppSideVWAP = openVWAP[oppSideIndex] / std::abs(oldPosition);
                openVWAP[oppSideIndex] = oppSideVWAP / std::abs(position);
                realPnL += std::min(static_cast<int32_t>(clientResponse->execQty), std::abs(oldPosition)) * (oppSideVWAP - clientResponse->price) * sideToValue(clientResponse->side);
            
                if (position * oldPosition < 0) { // flipped position to opposite sign
                    openVWAP[sideIndex] = clientResponse->price * std::abs(position);
                    openVWAP[oppSideIndex] = 0;
                }
            }

            if (!position) { // flat
                openVWAP[sideToIndex(Side::Buy)] = openVWAP[sideToIndex(Side::Sell)] = 0;
                unrealPnL = 0;
            } else {
                if (position > 0)
                    unrealPnL = (clientResponse->price - openVWAP[sideToIndex(Side::Buy)] / std::abs(position)) *std::abs(position);
                else
                    unrealPnL = (openVWAP[sideToIndex(Side::Sell)] / std::abs(position) - clientResponse->price) *std::abs(position);
            }

            totalPnL = unrealPnL + realPnL;
            std::string timeStr;
            logger->log("%:% %() % % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&timeStr), toString(), clientResponse->toString().c_str());
        }

        // functio to be called when there was a market update for the trading instrument
        auto updateBBO(const BBO* bbo, Logger* logger) noexcept -> void {
            std::string timeStr;
            bbo = bbo;
            if (position && bbo->bidPrice != Price_INVALID && bbo->askPrice != Price_INVALID) {
                const auto midPrice = (bbo->bidPrice + bbo->askPrice) * 0.5;
                if (position > 0)
                    unrealPnL = (midPrice - openVWAP[sideToIndex(Side::Buy)] / std::abs(position)) * std::abs(position);
                else
                    unrealPnL = (openVWAP[sideToIndex(Side::Sell)] / std::abs(position) - midPrice) * std::abs(position);
         
                const auto oldTotalPnL = totalPnL;
                totalPnL = unrealPnL + realPnL;

                if (totalPnL != oldTotalPnL) {
                    logger->log("%:% %() % % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&timeStr), toString(), bbo->toString());
                }
            }
        }
    };

    class PositionKeeper {
    public:
        PositionKeeper(Common::Logger* logger) : _logger(logger) {} 
        
        PositionKeeper() = delete;
        PositionKeeper(const PositionKeeper &) = delete;
        PositionKeeper(const PositionKeeper &&) = delete;
        PositionKeeper &operator=(const PositionKeeper &) = delete;
        PositionKeeper &operator=(const PositionKeeper &&) = delete;

        auto addFill(const Exchange::MEClientResponse* clientResponse) noexcept {
            _tickerPositions.at(clientResponse->tickerId).addFill(clientResponse, _logger);
        }

        auto updateBBO(TickerId tickerId, const BBO* bbo) noexcept {
            _tickerPositions.at(tickerId).updateBBO(bbo, _logger);
        }

        auto getPositionInfo(TickerId tickerId) const noexcept {
            return &_tickerPositions.at(tickerId);
        }

        auto toString() const {
            double total_pnl = 0;
            Qty total_vol = 0;

            std::stringstream ss;

            for(TickerId i = 0; i < _tickerPositions.size(); ++i) {
                ss << "TickerId:" << tickerIdToString(i) << " " << _tickerPositions.at(i).toString() << "\n";
                total_pnl += _tickerPositions.at(i).totalPnL;
                total_vol += _tickerPositions.at(i).volume;
            }

            ss << "Total PnL:" << total_pnl << " Vol:" << total_vol << "\n";
            return ss.str();
        }

    private:
        std::string _timeStr;
        Common::Logger* _logger = nullptr;
        std::array<PositionInfo, ME_MAX_TICKERS> _tickerPositions;

    };
}