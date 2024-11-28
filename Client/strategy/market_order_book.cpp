#include "market_order_book.h"
#include "trading_engine.h"

namespace Trading {
    MarketOrderBook::MarketOrderBook(TickerId tickerId, Logger* logger) : _tickerId(tickerId), _logger(logger), _ordersAtPricePool(ME_MAX_PRICE_LEVELS), _orderPool(ME_MAX_ORDER_IDS) 
    {}

    MarketOrderBook::~MarketOrderBook() {
        _logger->log("%:% %() % OrderBook\n%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), toString(false, true));
        _tradingEngine = nullptr;
        _bidsByPrice = _asksByPrice = nullptr;
        _oidToOrder.fill(nullptr);
    }

    auto MarketOrderBook::setTradingEngine(TradingEngine* tradingEngine) noexcept -> void {
        _tradingEngine = tradingEngine;
    }

    auto MarketOrderBook::getBBO() const noexcept -> const BBO* {
        return &_bbo;
    }

    auto MarketOrderBook::onMarketUpdate(const Exchange::MEMarketUpdate* marketUpdate) noexcept -> void {
        // see wether or not we need to update BBO
        const auto bidsUpdated = (_bidsByPrice && marketUpdate->side == Side::Buy && marketUpdate->price >= _bidsByPrice->price);
        const auto askUpdated = (_asksByPrice && marketUpdate->side == Side::Sell && marketUpdate->price >= _asksByPrice->price);

        switch (marketUpdate->type) {
            case Exchange::MarketUpdateType::ADD: {
                auto order = _orderPool.allocate(marketUpdate->orderId, marketUpdate->side, marketUpdate->price, marketUpdate->qty, marketUpdate->priority, nullptr, nullptr);
                addOrder(order);
            }
                break;
            case Exchange::MarketUpdateType::MODIFY: {
                auto order = _oidToOrder.at(marketUpdate->orderId);
                order->qty = marketUpdate->qty;
            }
                break;
            case Exchange::MarketUpdateType::CANCEL: {
                auto order = _oidToOrder.at(marketUpdate->orderId);
                removeOrder(order);
            }
                break;
            case Exchange::MarketUpdateType::TRADE: {
                _tradingEngine->onTradeUpdate(marketUpdate, this);
                return;
            }
                break;
            case Exchange::MarketUpdateType::CLEAR: {
                // clear orderBook for sync up to happen
                for (auto &order : _oidToOrder) {
                    if (order) {
                        _orderPool.deallocate(order);
                    }
                }

                _oidToOrder.fill(nullptr);

                if (_bidsByPrice) {
                    for (auto bid = _bidsByPrice->nextEntry; bid != _bidsByPrice; bid = bid->nextEntry)
                        _ordersAtPricePool.deallocate(bid);
                    _ordersAtPricePool.deallocate(_bidsByPrice);
                }
                
                if (_asksByPrice) {
                    for (auto ask = _asksByPrice->nextEntry; ask != _asksByPrice; ask = ask->nextEntry)
                        _ordersAtPricePool.deallocate(ask);
                    _ordersAtPricePool.deallocate(_asksByPrice);
                }

                _bidsByPrice = _asksByPrice = nullptr;
            }
                break;
            case Exchange::MarketUpdateType::INVALID:
            case Exchange::MarketUpdateType::SNAPSHOT_START:
            case Exchange::MarketUpdateType::SNAPSHOT_END:
                break;
        }

        updateBBO(bidsUpdated, askUpdated);

        // notify trading engine that order book was updated successfully
        _logger->log("%:% %() % % %", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), marketUpdate->toString(), _bbo.toString());
        _tradingEngine->onOrderBookUpdate(marketUpdate->tickerId, marketUpdate->price, marketUpdate->side, this);
    }

    auto MarketOrderBook::updateBBO(bool updateBid, bool updateAsks) noexcept {
        if (updateBid) {
            if (_bidsByPrice) { 
                _bbo.bidPrice = _bidsByPrice->firstMarketOrder->price;
                _bbo.bidQty = _bidsByPrice->firstMarketOrder->qty;
                
                for (auto order = _bidsByPrice->firstMarketOrder->nextOrder; order != _bidsByPrice->firstMarketOrder; order = order->nextOrder)
                    _bbo.bidQty += order->qty;
            } else {
                _bbo.bidPrice = Price_INVALID 
                _bbo.bidQty = Qty_INVALID;
            }
        }

        if (updateAsks) {
            if (_asksByPrice) { 
                _bbo.askPrice = _asksByPrice->firstMarketOrder->price;
                _bbo.askQty = _asksByPrice->firstMarketOrder->qty;
                
                for (auto order = _asksByPrice->firstMarketOrder->nextOrder; order != _asksByPrice->firstMarketOrder; order = order->nextOrder)
                    _bbo.askPrice += order->qty;
            } else {
                _bbo.askPrice = Price_INVALID 
                _bbo.askQty = Qty_INVALID;
            }
        }
    }
    
    auto MarketOrderBook::getOrdersAtPrice(Price price) const noexcept {
        return _priceOrdersAtPrice.at(priceToIndex(price));
    }

    auto MarketOrderBook::priceToIndex(Price price) const noexcept -> void {
        return (price % ME_MAX_PRICE_LEVELS);
    }

    auto MarketOrderBook::removeOrder(MarketOrder* order) noexcept -> void {
        auto ordersAtPrice = getOrdersAtPrice(order->price);
        if (order->prevOrder == order) { // only one element at that price level, remove it
            removeOrdersAtPrice(order->side, order->price);
        } else { // remove the order from the price level 
            const auto orderBefore = order->prevOrder;
            const auto orderAfter = order->nextOrder;
            orderBefore->nextOrder = orderAfter;
            orderAfter->prevOrder = orderBefore;
            // if order removed was head of list update head
            if (ordersAtPrice->firstMeOrder == order) {
                ordersAtPrice->firstMeOrder = orderAfter;
            }
            order->prevOrder = order->nextOrder = order;
        }

        _cidOidToOrder.at(order->clientId).at(order->clientOrderId) = nullptr;
        _orderPool.deallocate(order);
    }

    auto MarketOrderBook::removeOrdersAtPrice(Side side, Price price) noexcept -> void {
        const auto bestOrdersAtPrice = (side == Side::Buy? _bidsByPrice : _asksByPrice);
        
        auto ordersAtPrice = getOrdersAtPrice(price);
        if (UNLIKELY(ordersAtPrice->nextEntry == ordersAtPrice)) { // only price at that price level
            (side == Side::Buy? _bidsByPrice : _asksByPrice) = nullptr;
        } else {
            ordersAtPrice->prevEntry->nextEntry = ordersAtPrice->nextEntry;
            ordersAtPrice->nextEntry->prevEntry = ordersAtPrice->prevEntry;
            
            if (ordersAtPrice == bestOrdersAtPrice) { // if price level removed was head for bids or asks list
                (side == Side::Buy? _bidsByPrice : _asksByPrice) = ordersAtPrice->nextEntry;
            }
            
            ordersAtPrice->prevEntry = ordersAtPrice->nextEntry = ordersAtPrice;
            _ordersAtPricePool.deallocate(ordersAtPrice);
        }
    }

    auto MarketOrderBook::addOrder(MarketOrder* order) noexcept -> void {
        const auto ordersAtPrice = getOrdersAtPrice(order->price);
        
        if (!ordersAtPrice) {
            // if there was not order at that price level before
            order->nextOrder = order->prevOrder = order;
            auto newOrderAtPrice = _ordersAtPricePool.allocate(order->side, order->price, order, nullptr, nullptr);
            addOrdersAtPrice(newOrderAtPrice);       
        } else {
            // add newly created order at the end of the list
            auto firstOrder = (ordersAtPrice ? ordersAtPrice->firstMeOrder : nullptr);
            firstOrder->prevOrder->nextOrder = order;
            order->prevOrder = firstOrder->prevOrder;
            order->nextOrder = nullptr;
            firstOrder->prevOrder = order;
            _cidOidToOrder.at(order->clientId).at(order->clientOrderId) = order;
        }
    }

    auto MarketOrderBook::addOrdersAtPrice(MarketOrderAtPrice* newOrdersAtPrice) noexcept -> void {
        _priceOrdersAtPrice.at(priceToIndex(newOrdersAtPrice->price)) = newOrdersAtPrice;

        // update bids or asks by price
        const auto bestOrdersByPrice = (newOrdersAtPrice->side == Side::Buy)? _bidsByPrice : _asksByPrice;
        if (UNLIKELY(!bestOrdersByPrice)) {
            (newOrdersAtPrice->side == Side::Buy? _bidsByPrice : _asksByPrice) = newOrdersAtPrice;
            newOrdersAtPrice->prevEntry = newOrdersAtPrice->nextEntry = newOrdersAtPrice;
        } else {
            // find correct entry on bids or asks to insert newOrdersAtPrice
            auto target = bestOrdersByPrice;
            bool addAfter = ((newOrdersAtPrice->side == Side::Sell && newOrdersAtPrice->price > target->price) || (newOrdersAtPrice->side == Side::Buy && newOrdersAtPrice->price < target->price));

            if (addAfter) {
                target = target->nextEntry;
                addAfter = ((newOrdersAtPrice->side == Side::Sell && newOrdersAtPrice->price > target->price) || (newOrdersAtPrice->side == Side::Buy && newOrdersAtPrice->price < target->price));
            }

            while(addAfter && target != bestOrdersByPrice) {
                addAfter = ((newOrdersAtPrice->side == Side::Sell && newOrdersAtPrice->price > target->price) || (newOrdersAtPrice->side == Side::Buy && newOrdersAtPrice->price < target->price));
                if (addAfter)
                    target = target->nextEntry;
            }

            // found correct spot to put new MEOrdersAtPrice
            if (addAfter) { // after target
                if (target == bestOrdersByPrice)
                    target = bestOrdersByPrice->prevEntry;
                newOrdersAtPrice->prevEntry = target;
                target->nextEntry->prevEntry = newOrdersAtPrice;        
                newOrdersAtPrice->nextEntry = target->nextEntry;
                target->nextEntry = newOrdersAtPrice;
            } else { // before target
                newOrdersAtPrice->prevEntry = target->prevEntry;
                newOrdersAtPrice->nextEntry = target;
                target->prevEntry->nextEntry = newOrdersAtPrice;
                target->prevEntry = newOrdersAtPrice;

                if ((newOrdersAtPrice->side == Side::Buy && newOrdersAtPrice->price > bestOrdersByPrice->price) ||
                    (newOrdersAtPrice->side == Side::Sell && newOrdersAtPrice->price < bestOrdersByPrice->price)) {
                target->nextEntry = (target->nextEntry == bestOrdersByPrice ? newOrdersAtPrice : target->nextEntry);
                (newOrdersAtPrice->side == Side::Buy ? _bidsByPrice : _asksByPrice) = newOrdersAtPrice;
                }
            }
        }
    }



}