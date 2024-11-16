#pragma once

#include <functional>
#include "macros.h"
#include "thread_utils.h"
#include "tcp_server.h"
#include "client_request.h"
#include "client_response.h"

namespace Trading {
    class OrderGateway {
    public:
        OrderGateway(ClientId clientId, Exchange::ClientRequestLFQueue* clientRequests, Exchange::ClientResponseLFQueue* clientResponses, std::string ip, const std::string iface, int port);
        ~OrderGateway();

        OrderGateway() = delete;
        OrderGateway(const OrderGateway&) = delete;
        OrderGateway(const OrderGateway&&) = delete;
        OrderGateway& operator=(const OrderGateway&) = delete;
        OrderGateway&& operator=(const OrderGateway&&) = delete;

        auto start() noexcept -> void;
        auto stop() noexcept -> void;

    private:
        auto run() noexcept -> void;
        auto recvCallback(TCPSocket* socket, Nanos rx_time) noexcept -> void;

        const ClientId _clientId;
        std::string _ip;
        const std::string _iface;
        const int _port = 0;
        // order requests comming from trading engine
        Exchange::ClientRequestLFQueue* _outgoingRequests = nullptr;
        // order responses going to trading engine
        Exchange::ClientResponseLFQueue* _incomingResponses = nullptr;
        volatile bool _run = false;
        std::string _timeStr;
        Logger _logger;
        size_t _nextOutgoingSeqNum = 1;
        size_t _nextExpSeqNum = 1;
        // socket to connect to exchange and send and receive messages
        Common::TCPSocket _tcpSocket;
    };
}