#pragma once

#include <functional>
#include "lf_queue.h"
#include "macros.h"
#include "tcp_server.h"
#include "client_request.h"
#include "client_response.h"
#include "fifo_sequencer.h"

namespace Exchange {
    // class representing order gateway server
    class OrderServer {
    public:
        OrderServer(ClientRequestLFQueue* clientRequests, ClientResponseLFQueue* clientResponses, const std::string& iface, int port);
        ~OrderServer();
        
        auto stop() noexcept -> void;
        auto run() noexcept -> void;

    private:
        auto start() noexcept -> void;

        auto recvCallback(TCPSocket *socket, Nanos rxTime) noexcept -> void;
        auto recvFinishedCallback() noexcept -> void;

        const std::string _iface;
        const int _port;
        // queue that receives responses by matching engine to be sent to client
        ClientResponseLFQueue* _outgoingResponses = nullptr;
        volatile bool _run = false;
        std::string _timeStr;
        Logger _logger;
        // tracks next seqNum to be sent to individual clients
        std::array<size_t, ME_MAX_CLIENTS> _cidNextOutgoingSeqNum;
        // tracks next seqNum to be expected by each client
        std::array<size_t, ME_MAX_CLIENTS> _cidNextExpSeqNum;
        // tracks tcp connections by clients
        std::array<Common::TCPSocket*, ME_MAX_CLIENTS> _cidTcpSocket;
        Common::TCPServer _server;
        FifoSequencer _fifoSequencer;
    };
}