#include "matching_engine.h"
#include "me_order_book.h"

namespace Exchange {
    MEOrderBook::MEOrderBook(TickerId tickerId, Logger* logger, MatchingEngine* matchingEngine):
        _tickerId(tickerId), _logger(logger), _matchingEngine(matchingEngine), 
        _ordersAtPricePool(ME_MAX_PRICE_LEVELS), _orderPool(ME_MAX_ORDER_IDS) {}

    MEOrderBook::~MEOrderBook() {
        _logger->log("%:% %() % OrderBook\n%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), toString(false, true));
        _matchingEngine = nullptr;
        _bidsByPrice = _asksByPrice = nullptr;
        for (auto &itr : _cidOidToOrder)
        {
            itr.fill(nullptr);
        }
    }

    auto MEOrderBook::generateNewMarketOrderId() noexcept -> OrderId {
        return _nextMarketOrderId++;
    }

    auto MEOrderBook::priceToIndex(Price price) const noexcept -> int {
        return (price % ME_MAX_PRICE_LEVELS);
    }
    
    auto MEOrderBook::getOrdersAtPrice(Price price) const noexcept {
        return _priceOrdersAtPrice.at(priceToIndex(price));
    }

    auto MEOrderBook::add(ClientId clientId, OrderId clientOrderId, TickerId tickerId, Side side, Price price, Qty qty) noexcept -> void {
        auto marketOrderId = generateNewMarketOrderId();
        _clientResponse = {ClientResponseType::ACCEPTED, clientId, tickerId, clientOrderId, marketOrderId, side, price, 0, qty};
        _matchingEngine->sendClientResponse(&_clientResponse);

        // check if we have a match against a passive order partial or full
        auto leavesQty = checkForMatch(clientId, clientOrderId, tickerId, side, price, qty, marketOrderId);

        // create new marketOrder if any qty is left out
        if (LIKELY(leavesQty)) {
            const auto priority = getNextPriority(price);
            auto order = _orderPool.allocate(tickerId, clientId, clientOrderId, marketOrderId, side, price, leavesQty, priority, nullptr, nullptr);
            addOrder(order);
            _marketUpdate = {MarketUpdateType::ADD, marketOrderId, tickerId, side, price, leavesQty, priority};
            _matchingEngine->sendMarketUpdate(&_marketUpdate);
        } 
    }


    auto MEOrderBook::getNextPriority(Price price) const noexcept -> uint64_t {
        const auto ordersAtPrice = getOrdersAtPrice(price);
        if (!ordersAtPrice)
            return 1lu;
        return ordersAtPrice->firstMeOrder->prevOrder->priority + 1;
    }

    auto MEOrderBook::addOrder(MEOrder* order) noexcept -> void {
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

    auto MEOrderBook::addOrdersAtPrice(MEOrderAtPrice* newOrdersAtPrice) noexcept -> void {
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

    auto MEOrderBook::cancel(ClientId clientId, OrderId orderId, TickerId tickerId) noexcept -> void {
        auto isCancable =  (clientId < _cidOidToOrder.size());
        MEOrder* exchangeOrder = nullptr;
        if (LIKELY(isCancable)) {
            auto &coItr = _cidOidToOrder.at(clientId);
            exchangeOrder = coItr.at(orderId);
            isCancable = (exchangeOrder != nullptr);
        }

        if (UNLIKELY(!isCancable)) {
            _clientResponse = {ClientResponseType::CANCEL_REJECTED, clientId, tickerId, orderId, OrderId_INVALID, Side::Invalid, Price_INVALID, Qty_INVALID, Qty_INVALID};
        } else {
            _clientResponse = {ClientResponseType::CANCELED, clientId, tickerId, orderId, exchangeOrder->marketOrderId, exchangeOrder->side, exchangeOrder->price, Qty_INVALID ,exchangeOrder->qty};
            _marketUpdate =  {MarketUpdateType::CANCEL, exchangeOrder->marketOrderId, tickerId, exchangeOrder->side, exchangeOrder->price, 0, exchangeOrder->priority};
            
            removeOrder(exchangeOrder);
            _matchingEngine->sendMarketUpdate(&_marketUpdate);
        }

        _matchingEngine->sendClientResponse(&_clientResponse);
    }

    auto MEOrderBook::removeOrder(MEOrder* order) noexcept -> void {
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

    auto MEOrderBook::removeOrdersAtPrice(Side side, Price price) noexcept -> void {
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

    auto MEOrderBook::checkForMatch(ClientId clientId, OrderId clientOrderId, TickerId tickerId, Side side, Price price, Qty qty, OrderId newMarketOrderId) noexcept -> Qty {
        auto leavesQty = qty;

        if (side == Side::Buy) {
            while (leavesQty && _asksByPrice)
            {
                const auto askItr = _asksByPrice->firstMeOrder;
                if (LIKELY(price < askItr->price))
                    break;

                match(tickerId, clientId, side, clientOrderId, newMarketOrderId, askItr, &leavesQty);
            }
        } 

        if (side == Side::Sell) {
            while (leavesQty && _bidsByPrice)
            {
                const auto bidsItr = _bidsByPrice->firstMeOrder;
                if (LIKELY(price > bidsItr->price))
                    break;

                match(tickerId, clientId, side, clientOrderId, newMarketOrderId, bidsItr, &leavesQty);
            }
        }

        return leavesQty;
    }

    auto MEOrderBook::match(TickerId tickerId, ClientId clientId, Side side, OrderId clientOrderId, OrderId newMarketOrderId, MEOrder* itr, Qty* leavesQty) noexcept -> void {
        const auto order = itr;
        const auto orderQty = order->qty;
        const auto fillQty = std::min(orderQty, *leavesQty);

        *leavesQty -= fillQty;
        order->qty -= fillQty;

        // response to the client who had the aggressive order
        _clientResponse = {ClientResponseType::FILLED, clientId, tickerId, clientOrderId, newMarketOrderId, side, itr->price, fillQty, *leavesQty};
        _matchingEngine->sendClientResponse(&_clientResponse);

        // response to the client who had the passive order
        _clientResponse = {ClientResponseType::FILLED, order->clientId, tickerId, order->clientOrderId, order->marketOrderId, order->side, itr->price, fillQty, order->qty};
        _matchingEngine->sendClientResponse(&_clientResponse);

        // notify other users of the market update comming from the aggresive order
        _marketUpdate = {MarketUpdateType::TRADE, OrderId_INVALID, tickerId, side, itr->price, fillQty, Priority_INVALID};
        _matchingEngine->sendMarketUpdate(&_marketUpdate);

        // notify other users of the market update comming from the passive order
        if(!order->qty) {
            _marketUpdate = {MarketUpdateType::CANCEL, order->marketOrderId, tickerId, order->side, order->price, orderQty, Price_INVALID};
            _matchingEngine->sendMarketUpdate(&_marketUpdate);
            removeOrder(order);
        } else {
            _marketUpdate = {MarketUpdateType::MODIFY, order->marketOrderId, tickerId, order->side, order->price, orderQty, order->priority};
            _matchingEngine->sendMarketUpdate(&_marketUpdate);
        }
    }

    auto MEOrderBook::toString(bool detailed, bool validity_check) const noexcept -> std::string {
        std::stringstream ss;
        std::string timeStr;

        auto printer = [&](std::stringstream &ss, MEOrderAtPrice* itr, Side side, Price& lastPrice, bool sanityCheck) {
            char buff[4096];
            Qty qty = 0;
            size_t numOfOrders = 0;

            for (auto orderItr = itr->firstMeOrder;; orderItr = orderItr->nextOrder) {
                qty += orderItr->qty;
                ++numOfOrders;

                if (orderItr->nextOrder == itr->firstMeOrder)
                    break;
            }

            // print general info about orders at price level
            sprintf(buff, " <px:%3s p:%3s n:%3s> %-3s @ %-5s(%-4s)", priceToString(itr->price).c_str(), priceToString(itr->prevEntry->price).c_str(), priceToString(itr->nextEntry->price).c_str(), priceToString(itr->price).c_str(), qtyToString(qty).c_str(), std::to_string(numOfOrders).c_str());
            ss << buff;

            // print individual orders
            for (auto orderItr = itr->firstMeOrder;; orderItr = orderItr->nextOrder) {
                if (detailed) {
                    sprintf(buff, "[oid:%s q:%s p:%s n:%s] ", orderIdToString(orderItr->marketOrderId).c_str(), qtyToString(orderItr->qty).c_str(), orderIdToString(orderItr->prevOrder ? orderItr->prevOrder->marketOrderId : OrderId_INVALID).c_str(), orderIdToString(orderItr->nextOrder ? orderItr->nextOrder->marketOrderId : OrderId_INVALID).c_str());
                    ss << buff;
                }

                if (orderItr->nextOrder == itr->firstMeOrder)
                    break;
            }

            ss << std::endl;

            // perform check that oders are in correct order
            if (sanityCheck) {
                if ((side == Side::Sell && lastPrice >= itr->price) || (side == Side::Buy && lastPrice <= itr->price)) {
                    FATAL("Bids/Asks not sorted by ascending/descending prices last:" + priceToString(lastPrice) + " itr:" + itr->toString());
                }
            }
        };

        // print asks
        ss << "Ticker: " << tickerIdToString(_tickerId) << std::endl;
        {
            auto askItr = _asksByPrice;
            auto lastAskPrice = std::numeric_limits<Price>::min();
            for (size_t count = 0; askItr; ++count) {
                ss << "ASKS L:" << count << " => ";
                auto nextAskItr = (askItr->nextEntry == _asksByPrice? nullptr : askItr->nextEntry);
                printer(ss, askItr, Side::Sell, lastAskPrice, validity_check);
                askItr = nextAskItr;
            }
        }

        ss << std::endl << "                          X" << std::endl << std::endl;

        // print bids
        {
            auto bidsItr = _bidsByPrice;
            auto lastAskPrice = std::numeric_limits<Price>::max();
            for (size_t count = 0; bidsItr; ++count) {
                ss << "ASKS L:" << count << " => ";
                auto nextBidsItr = (bidsItr->nextEntry == _bidsByPrice? nullptr : bidsItr->nextEntry);
                printer(ss, bidsItr, Side::Sell, lastAskPrice, validity_check);
                bidsItr = nextBidsItr;
            }
        }

        return ss.str();
    }
}