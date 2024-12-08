#include "trading_engine.h"

namespace Trading {
    TradingEngine::TradingEngine(ClientId clientId, AlgoType algoType, const TradingEngineCfgHashMap& tickerCfg, Exchange::ClientRequestLFQueue* clientRequests,
    Exchange::ClientResponseLFQueue* clientResponse, Exchange::MEMarketUpdateLFQueue* marketUpdates) 
    : _clientId(clientId), _outgoingOgwRequests(clientRequests), _incomingOgwResponses(clientResponse), 
    _incomingMdUpdates(marketUpdates), _logger("trading_engine_" + std::to_string(clientId) + ".log"),
    _featureEngine(&_logger), _positionKeeper(&_logger), _riskManager(&_logger, &_positionKeeper, tickerCfg), _orderManager(&_logger, this, _riskManager) {
        for (size_t i = 0; i < _tickerOrderBook.size() ; i++)
        {
            _tickerOrderBook[i] = new MarketOrderBook(i, &_logger);
            _tickerOrderBook[i]->setTradingEngine(this);
        }
        

        algoOnOrderBookUpdate = [this](TickerId tickerId, Price price, Side side, MarketOrderBook *book) {
            defaultAlgoOnOrdeBookUpdate(tickerId, price, side, book);
        };

        algoOnTradeUpdate = [this](const Exchange::MEMarketUpdate *marketUpdate, MarketOrderBook *book) {
            defaultAlgoOnTradeUpdate(marketUpdate, book);
        };

        algoOnOrderUpdate = [this](const Exchange::MEClientResponse *clientResponse) {
            defaultAlgoOnOrderUpdate(clientResponse);
        };

        // depending on algo type use the initialize the correct algorithm
        if (algoType == AlgoType::MAKE) {
            _mmAlgo = new MarketMaker(&_logger, this, &_featureEngine, &_orderManager, tickerCfg);        
        } else if (algoType == AlgoType::TAKER) {
            _takerAlgo = new LiquidityTaker(&_logger, this, &_featureEngine, &_orderManager, tickerCfg);
        }

        for (TickerId i = 0; i < tickerCfg.size(); ++i) {
            _logger.log("%:% %() % Initialized % Ticker:% %.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), algoTypeToString(algoType), i, tickerCfg.at(i).toString());
        }
    }

    TradingEngine::~TradingEngine() {
        _run = false;

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);

        delete _mmAlgo; 
        _mmAlgo = nullptr;
        delete _takerAlgo; 
        _takerAlgo = nullptr;

        for (auto &order_book: _tickerOrderBook) {
            delete order_book;
            order_book = nullptr;
        }

        _outgoingOgwRequests = nullptr;
        _incomingOgwResponses = nullptr;
        _incomingMdUpdates = nullptr;
    }

    auto TradingEngine::start() noexcept -> void {
        _run = true;
        ASSERT(createAndStartThread(-1, "Trading/TradingEngine", [this](){run();}) != nullptr, "faild to start Trading Engine.");
    }
    

    auto TradingEngine::stop() noexcept -> void {
        while(_incomingMdUpdates->size() || _incomingOgwResponses->size()) {
            _logger.log("%:% %() % Sleeping till all updates are consumed ogw-size:% md-size:%\n", __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&_timeStr), _incomingOgwResponses->size(), _incomingMdUpdates->size());
        
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }

        _logger.log("%:% %() % POSITIONS\n%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), _positionKeeper.toString());
        _run = false;
    }

    auto TradingEngine::sendClientRequest(const Exchange::MEClientRequest* clientRequest) noexcept -> void {
        _logger.log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), clientRequest->toString().c_str());

        // write request to order gateways server
        auto nextWrite = _outgoingOgwRequests->getNextWriteTo();
        *nextWrite = std::move(*clientRequest);
        _outgoingOgwRequests->updateWriteIndex();
    }

    auto TradingEngine::run() noexcept -> void {
        _logger.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr));
    
        while(_run) {
            for (auto res = _incomingOgwResponses->getNextRead(); res; res = _incomingOgwResponses->getNextRead())
            {
                _logger.log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), res->toString().c_str());
                onOrderUpdate(res);
                _incomingOgwResponses->updateReadIndex();
                _lastEventTime = Common::getCurrentNanos();
            }

            for (auto update = _incomingMdUpdates->getNextRead(); update; update = _incomingMdUpdates->getNextRead()) {
                _logger.log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), update->toString().c_str());

                // make sure we received valid ticker id
                ASSERT(update->tickerId < _tickerOrderBook.size(), "Unknown tickerId on update: " + update->tickerId);

                _tickerOrderBook[update->tickerId]->onMarketUpdate(update);
                _incomingMdUpdates->updateReadIndex();
                _lastEventTime = Common::getCurrentNanos();
            }
        }
    };

    auto TradingEngine::onOrderBookUpdate(TickerId tickerId, Price price, Side side, MarketOrderBook* book) noexcept -> void {
        _logger.log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), tickerId, Common::priceToString(price).c_str(), Common::sideToString(side).c_str());

        auto bbo = book->getBBO();        
        _positionKeeper.updateBBO(tickerId, bbo);
        _featureEngine.onOrderBookUpdate(tickerId, price, side, book);
        algoOnOrderBookUpdate(tickerId, price, side, book);
    }

    auto TradingEngine::onTradeUpdate(const Exchange::MEMarketUpdate *marketUpdate, MarketOrderBook *book) noexcept -> void {
        _logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), marketUpdate->toString().c_str());
    
        _featureEngine.onTradeUpdate(marketUpdate, book);
        algoOnTradeUpdate(marketUpdate, book);
    }

    auto TradingEngine::onOrderUpdate(const Exchange::MEClientResponse *response) noexcept -> void {
        _logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), response->toString().c_str());

        if (UNLIKELY(response->type == Exchange::ClientResponseType::FILLED))
            _positionKeeper.addFill(response);

        algoOnOrderUpdate(response);
    }

    auto TradingEngine::initLastEventTime() noexcept -> void {
        _lastEventTime = Common::getCurrentNanos();
    }

    auto TradingEngine::silenceSeconds() const noexcept ->  int64_t {
        return (Common::getCurrentNanos() - _lastEventTime);
    }

    auto TradingEngine::getClientId() const noexcept -> ClientId {
        return _clientId;
    }

}