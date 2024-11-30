#pragma once

#include "macros.h"
#include "logging.h"
#include "order_manager.h"
#include "feature_engine.h"

using namespace Common;

namespace Trading {
    class MarketMaker {
    public:
        MarketMaker(Logger* logger, TradingEngine* tradingEngine, const FeatureEngine* featureEngine, OrderManager* orderManager,
        const TradingEngineCfgHashMap& tickerCfg): _featureEngine(featureEngine), _orderManager(orderManager), _logger(logger), _tickerCfg(tickerCfg) {
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

        MarketMaker() = delete;
        MarketMaker(const MarketMaker &) = delete;
        MarketMaker(const MarketMaker &&) = delete;
        MarketMaker &operator=(const MarketMaker &) = delete;
        MarketMaker &operator=(const MarketMaker &&) = delete;

        auto onOrderBookUpdate(TickerId tickerId, Price price, Side side, const MarketOrderBook* book) noexcept -> void {
            _logger->log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&_timeStr), tickerId, Common::priceToString(price).c_str(),
                Common::sideToString(side).c_str());

            const auto bbo = book->getBBO();
            const auto fairPrice = _featureEngine->getMktPrice();

            if (LIKELY(bbo->askPrice != Price_INVALID && bbo->bidPrice != Price_INVALID && fairPrice != Price_INVALID)) {
                const auto clip = _tickerCfg.at(tickerId).clip;
                const auto threshold = _tickerCfg.at(tickerId).threshold;

                const auto bidPrice = bbo->bidPrice - (fairPrice - bbo->bidPrice >= threshold? 0 : 1);
                const auto askPrice = bbo->askPrice - (fairPrice - bbo->askPrice >= threshold? 0 : 1);
                
                _orderManager->moveOrders(tickerId, bidPrice, askPrice, clip);
            }
        }

        auto onTradeUpdate(const Exchange::MEMarketUpdate* marketUpdate, MarketOrderBook* book) noexcept -> void {
            _logger->log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), marketUpdate->toString().c_str());
        }
        
        auto onOrderUpdate(const Exchange::MEClientResponse* clientResponse) noexcept -> void  {
            _logger->log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), clientResponse->toString().c_str());
            _orderManager->onOrderUpdate(clientResponse);
        }

    private:
        const FeatureEngine* _featureEngine = nullptr;
        OrderManager* _orderManager = nullptr;
        std::string _timeStr;
        Logger* _logger;
        const TradingEngineCfgHashMap _tickerCfg;
    };
}