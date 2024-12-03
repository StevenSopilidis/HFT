#pragma once

#include "macros.h"
#include "logging.h"
#include "order_manager.h"
#include "feature_engine.h"

using namespace Common;

namespace Trading {
    class LiquidityTaker {
    public:
        LiquidityTaker(Logger* logger, TradingEngine* tradingEngine, FeatureEngine* FeatureEngine, OrderManager* orderManager,
        const TradingEngineCfgHashMap& tickerCfg) : _featureEngine(_featureEngine), _logger(logger), _orderManager(orderManager), 
        _tickerCfg(tickerCfg) {
            tradingEngine->algoOnOrderBookUpdate = [this](auto tickerId, auto price, auto side, auto book) {
                onOrderBookUpdate(tickerId, price, side, book);
            };

            tradingEngine->algoOnTradeUpdate = [this](auto marketUpdate, auto book) {
                onTradeUpdate(marketUpdate, book);
            };

            tradingEngine->algoOnOrderUpdate = [this](auto clientResponse) {
                onOrderUpdate(clientResponse);
            };
        }

        LiquidityTaker() = delete;
        LiquidityTaker(const LiquidityTaker &) = delete;
        LiquidityTaker(const LiquidityTaker &&) = delete;
        LiquidityTaker &operator=(const LiquidityTaker &) = delete;
        LiquidityTaker &operator=(const LiquidityTaker &&) = delete;

        auto onTradeUpdate(const Exchange::MEMarketUpdate* marketUpdate, MarketOrderBook* book) noexcept -> void {
            _logger->log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), marketUpdate->toString().c_str());
        
            const auto bbo = book->getBBO();
            const auto aggQtyRatio = _featureEngine->getAggTradeQtyRatio();

            if (LIKELY(bbo->bidPrice != Price_INVALID && bbo->askPrice != Price_INVALID && aggQtyRatio != Feature_INVALID)) {
                _logger->log("%:% %() % % agg-qty-ratio:%\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr),bbo->toString().c_str(), aggQtyRatio);
            
                auto clip = _tickerCfg.at(marketUpdate->tickerId).clip;
                auto threshold = _tickerCfg.at(marketUpdate->tickerId).threshold;

                if (aggQtyRatio >= threshold) {
                    if (marketUpdate->side == Side::Buy)
                        _orderManager->moveOrders(marketUpdate->tickerId, bbo->askPrice, Price_INVALID, clip);
                } else {
                    _orderManager->moveOrders(marketUpdate->tickerId, Price_INVALID, bbo->bidPrice, clip);
                }
            } 
        }

        auto onOrderBookUpdate(TickerId ticker_id, Price price, Side side, MarketOrderBook *) noexcept -> void {
            _logger->log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), ticker_id, priceToString(price).c_str(), sideToString(side).c_str());
        }

        auto onOrderUpdate(const Exchange::MEClientResponse *client_response) noexcept -> void {
            _logger
            ->log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&_timeStr), client_response->toString().c_str());
            
            _orderManager->onOrderUpdate(client_response);
        }



    private:
        const FeatureEngine* _featureEngine = nullptr;
        OrderManager* _orderManager = nullptr;
        std::string _timeStr;
        Logger* _logger;
        const TradingEngineCfgHashMap _tickerCfg;

    };
}