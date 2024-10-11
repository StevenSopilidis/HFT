#include "matching_engine.h"

namespace Exchange {
    MatchingEngine::MatchingEngine(ClientRequestLFQueue* clientRequests, ClientResponseLFQueue* clientResponses, MEMarketUpdateLFQueue* marketUpdates) :
    _incomingRequests(clientRequests), _outgoingOgwResponses(clientResponses), _outgoingMDUpdates(marketUpdates),
    _logger("exchange_matching_engine.log") {
        for (size_t i = 0; i < _tickerOrderBook.size(); i++)
        {
            _tickerOrderBook[i] = new MEOrderBook(i, &_logger, this);
        }
    }

    MatchingEngine::~MatchingEngine() {
        stop();
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
        _incomingRequests = nullptr;
        _outgoingOgwResponses = nullptr;
        _outgoingMDUpdates = nullptr;
        for (auto &orderBook : _tickerOrderBook) {
            delete orderBook;
            orderBook = nullptr;
        }
    }

    auto MatchingEngine::stop() noexcept -> void {
        _run = false;
    }

    auto MatchingEngine::run() noexcept -> void {
        _logger.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr));

        while(_run) {
            const auto clientRequest = _incomingRequests->getNextRead();
            if (LIKELY(clientRequest)) {
                _logger.log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), clientRequest->toString());
                processClientRequest(clientRequest);
                _incomingRequests->updateReadIndex();
            }
        }
    }

    auto MatchingEngine::processClientRequest(const MEClientRequest* clientRequest) noexcept -> void {
        auto orderBook = _tickerOrderBook[clientRequest->tickerId];

        switch (clientRequest->type)
        {
        case ClientRequestType::NEW:
            orderBook->add(clientRequest->clientId, clientRequest->orderId, clientRequest->tickerId, clientRequest->side, clientRequest->price, clientRequest->qty);
            break;
        case ClientRequestType::CANCEL:
            orderBook->cancel(clientRequest->clientId, clientRequest->orderId, clientRequest->tickerId);
        default:
            FATAL("Received invalid client-request type: " + clientRequestTypeToString(clientRequest->type));
            break;
        }
    }

    auto MatchingEngine::start() noexcept -> void {
        _run = true;
        ASSERT(Common::createAndStartThread(-1, "Exchange/MatchingEngine", [this]() { run(); }) != nullptr, "Failed to start MatchingEngine thread.");
    }


    auto MatchingEngine::sendClientResponse(const MEClientResponse* clientResponse) noexcept -> void {
        _logger.log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), clientResponse->toString());
        auto nextWrite = _outgoingOgwResponses->getNextWriteTo();
        *nextWrite = std::move(*clientResponse);
        _outgoingOgwResponses->updateWriteIndex();
    }
}
