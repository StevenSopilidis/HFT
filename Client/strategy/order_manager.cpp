#include "order_manager.h"


namespace Trading {
    OrderManager::OrderManager(Logger* logger, TradingEngine* tradingEngine, RiskManager& riskManager)
    : _Logger(logger), _tradingEngine(tradingEngine), _riskManager(riskManager) {}


    auto OrderManager::newOrder(OMOrder* order, TickerId tickerId, Price price, Side side, Qty qty) noexcept -> void {
        const Exchange::MEClientRequest newRequest{Exchange::ClientRequestType::NEW,  _tradingEngine->clientId(), tickerId, _nextOrderId, side, price, qty};

        _tradingEngine->sendClientRequest(&newRequest);
        
        *order = {tickerId, _nextOrderId, side, price, qty, OMOrderState::PENDING_NEW};
        _nextOrderId++;
        _logger->log("%:% %() % Sent new order % for %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), newRequest.toString().c_str(), order->toString().c_str());
    }

    auto OrderManager::cancelOrder(OMOrder* order) noexcept -> void {
        const Exchange::MEClientRequest cancelRequest{Exchange::ClientRequestType::NEW,  _tradingEngine->clientId(), order->tickerId, order->orderId, order->side, order->price, order->qty};
        
        _tradingEngine->sendClientRequest(&cancelRequest);

        order->orderState = OMOrderState::PENDING_CANCEL;
        _logger->log("%:% %() % Sent new order % for %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), cancelRequest.toString().c_str(), order->toString().c_str());
    }

    auto OrderManager::moveOrder(OMOrder* order, TickerId tickerId, Price price, Side side, Qty qty) noexcept -> void {
        switch (order->orderState)
        {
        case OMOrderState::LIVE : {
            if (order->price != price || order->qty != qty)
                cancelOrder(order);
            break;
        }
        case OMOrderState::INVALID:
        case OMOrderState::DEAD: {
            if (LIKELY(price != Price_INVALID)) {
                const auto riskResult = _riskManager.checkPreTradeRisk(tickerId, side, qty);
                if LIKELY(riskResult == RiskCheckResult::ALLOWED) 
                    newOrder(order, tickerId, price, side, qty);
                else
                _logger->log("%:% %() % Ticker:% Side:% Qty:% RiskCheckResult:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),tickerIdToString(ticker_id), sideToString(side), qtyToString(qty), riskCheckResultToString(risk_result));
            }
            break;
        } 
        case OMOrderState::PENDING_NEW:
        case OMOrderState::PENDING_CANCEL:
            break;
        }
    }

    auto OrderManager::moveOrders(TickerId tickerId, Price bidPrice, Price askPrice, Qty clip) noexcept -> void {
        auto bidOrder = &(_tickerSideOrder.at(tickerId).at(sideToIndex(Side::Buy)));
        moveOrder(bidOrder, tickerId, bidPrice, Side::Buy, clip);

        auto askOrder = &(_tickerSideOrder.at(tickerId).at(sideToIndex(Side::Buy)));
        moveOrder(askOrder, tickerId, bidPrice, Side::Sell, clip);
    }

    auto OrderManager::onOrderUpdate(const Exchange::MEClientResponse* clientResponse) noexcept -> void {
        _logger->log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), clientResponse->toString().c_str());
        
        auto order = &(ticker_side_order_.at(client_response->ticker_id_).at(sideToIndex(client_response->side_)));
        _logger->log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), order->toString().c_str());
    
        switch (clientResponse->type) {
            case Exchange::ClientResponseType::ACCEPTED: {
                order->orderState = OMOrderState::LIVE;
            }
            break;
            case Exchange::ClientResponseType::CANCELED: {
                order->orderState = OMOrderState::DEAD;
            }
            break;
            case Exchange::ClientResponseType::FILLED: {
                order->qty_ = client_response->leaves_qty_;
                if(!order->qty_)
                    order->orderState = OMOrderState::DEAD;
                }
            break;
            case Exchange::ClientResponseType::CANCEL_REJECTED:
            case Exchange::ClientResponseType::INVALID: {
            }
            break;
        }
    }

}   