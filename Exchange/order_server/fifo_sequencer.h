#pragma once

#include "thread_utils.h"
#include "macros.h"
#include "client_request.h"

namespace Exchange {
    constexpr size_t ME_MAX_PENDING_REQUESTS = 1024; // max number of pending cllient requests

    class FifoSequencer {
    public:
        FifoSequencer(ClientRequestLFQueue* queue, Logger* logger) : _incomingRequests(queue), _logger(logger) {}

        // add requests to pending requests array
        auto addClientRequest(Nanos rxTime, const MEClientRequest& request) noexcept {
            if (_pendingSize >= ME_MAX_PENDING_REQUESTS)
                FATAL("Too many pending requests");
            
            _pendingRequests.at(_pendingSize++) = std::move(RecvTimeClientRequest{rxTime, request});
        }

        // sorts requests by time and sends them for processing to matching engine via queue
        auto sequenceAndPublish() noexcept {
            if (UNLIKELY(!_pendingSize))
                return;

            _logger->log("%:% %() % Processing % requests.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), _pendingSize);

            std::sort(_pendingRequests.begin(), _pendingRequests.begin() + _pendingSize);

            for (size_t i = 0; i < _pendingSize; i++) {
                const auto request = _pendingRequests.at(i);

                _logger->log("%:% %() % Writing RX:% Req:% to FIFO.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), request.recvTime, request.request.toString());

                auto nextWrite = _incomingRequests->getNextWriteTo();
                *nextWrite = std::move(request.request);
                _incomingRequests->updateWriteIndex();
            }
            
            _pendingSize = 0;
        }

    private:
        ClientRequestLFQueue *_incomingRequests = nullptr;
        std::string _timeStr;
        Logger* _logger = nullptr;

        // struct representing client request & time it was sent
        struct RecvTimeClientRequest{
            Nanos recvTime = 0;
            MEClientRequest request;

            // for checking which request was sent first
            auto operator<(RecvTimeClientRequest &rhs) const noexcept {
                return (recvTime < rhs.recvTime);
            }
        };

        std::array<RecvTimeClientRequest, ME_MAX_PENDING_REQUESTS> _pendingRequests;
        size_t _pendingSize = 0;
    };
}