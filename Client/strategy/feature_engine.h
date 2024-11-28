#pragma once

#include "logging.h"
#include "macros.h"
#include "types.h"
#include "market_order_book.h"

using namespace Common;

namespace Trading {
    constexpr auto Feature_INVALID = std::numeric_limits<double>::quiet_NaN();

    class FeatureEngine {
    public:
        FeatureEngine(Logger* logger) : _logger(logger) {}
    
        auto getMktPrice() const noexcept {
            return _mktPrice;
        }

        auto getAggTradeQtyRatio() const noexcept {
            return _aggTradeQtyRatio;
        }

        // method to be called when there is an update to order book
        auto onOrderBookUpdate(TickerId tickerId, Price price, Side side, MarketOrderBook* book) noexcept -> void {
            auto bbo = book->getBBO();
            if (LIKELY(bbo->bidPrice != Price_INVALID && bbo->bidPrice != Price_INVALID)) {
                _mktPrice = (bbo->bidPrice * bbo->askQty + bbo->askPrice * bbo->bidQty) / static_cast<double>(bbo->bidQty + bbo->askQty);
            }

            _logger->log("%:% %() % ticker:% price:% side:% mkt-price:% agg-trade-ratio:%\n", __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&_timeStr), tickerId, Common::priceToString(price).c_str(), Common::sideToString(side).c_str(), _mktPrice, _aggTradeQtyRatio);
        }

        // method to be called when there is an trade event
        auto onTradeUpdate(const Exchange::MEMarketUpdate* marketUpdate, MarketOrderBook* book) noexcept -> void {
            auto bbo = book->getBBO();
            if (LIKELY(bbo->bidPrice != Price_INVALID && bbo->askPrice != Price_INVALID)) {
                // try to compute was big a trade aggressor was compared to how much liquidity was available
                _aggTradeQtyRatio = static_cast<double>(marketUpdate->qty) / (marketUpdate->side == Side::Buy? bbo->askQty : bbo->bidQty);
            }

            _logger->log("%:% %() % % mkt-price:% agg-trade-ratio:%\n", __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&_timeStr), marketUpdate->toString().c_str(), _mktPrice, _aggTradeQtyRatio);
        };

    private:
        std::string _timeStr;
        Logger* _logger;
        double _mktPrice = Feature_INVALID; // used to compute fair market price
        double _aggTradeQtyRatio = Feature_INVALID; // used to compute aggresive trade quantity ratio
    };
}