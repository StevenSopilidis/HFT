#include "order_gateway.h"

namespace Trading {
    OrderGateway::OrderGateway(ClientId clientId, Exchange::ClientRequestLFQueue* clientRequests, Exchange::ClientResponseLFQueue* clientResponses, std::string ip, const std::string iface, int port) 
    : _clientId(clientId), _ip(ip), _iface(iface), _port(port), _outgoingRequests(clientRequests),
      _incomingResponses(clientResponses),  _logger("trading_order_gateway" + std::to_string(clientId) + ".log"),
      _tcpSocket(_logger)
    {
        _tcpSocket.recv_callback = [this](auto socket, auto rx_time) {recvCallback(socket, rx_time);};    
    }

    auto OrderGateway::start() noexcept -> void {
        _run = true;
        
        ASSERT(_tcpSocket.connect(_ip, _iface, _port, false) >= 0, "Unable to connect to ip: " + _ip + " port: " + std::to_string(_port) + " on iface: " + _iface + " error: " + std::string(std::strerror(errno)));
        ASSERT(Common::createAndStartThread(-1, "Trading/OrderGateway", [this](){run();}) != nullptr, "Failed to start OrderGateway thread.");
    }

    OrderGateway::~OrderGateway() {
        stop();
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(5s);
    }

    auto OrderGateway::stop() noexcept -> void {
        _run = false;
    }

    auto OrderGateway::run() noexcept -> void {
        _logger.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr));
        while(_run) {
            _tcpSocket.sendAndRecv();

            // loop throught requests and dispatch them
            for (auto clientRequest = _outgoingRequests->getNextRead(); clientRequest; clientRequest = _outgoingRequests->getNextRead()) {
                _logger.log("%:% %() % Sending cid:% seq:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), _clientId, _nextOutgoingSeqNum, clientRequest->toString());
                
                _tcpSocket.send(&_nextOutgoingSeqNum, sizeof(_nextOutgoingSeqNum));
                _tcpSocket.send(clientRequest, sizeof(Exchange::MEClientRequest));
                _outgoingRequests->updateReadIndex();
                _nextOutgoingSeqNum++;
            }
        }
    }

    auto OrderGateway::recvCallback(TCPSocket* socket, Nanos rx_time) noexcept -> void {
        _logger.log("%:% %() % Received socket:% len:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), socket->fd,socket->next_recv_valid_index, rx_time);
        if (socket->next_recv_valid_index >= sizeof(Exchange::OMClientResponse)) {
            size_t i = 0;
            for (; i + sizeof(Exchange::OMClientResponse) <= socket->next_recv_valid_index; i += sizeof(Exchange::OMClientResponse)) {
                auto response = reinterpret_cast<Exchange::OMClientResponse*>(socket->recv_buffer.data() + i);
                _logger.log("%:% %() % Received %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), response->toString());
            
                if (response->meClientResponse.clientId != _clientId) {
                    _logger.log("%:% %() % ERROR Incorrect client id. ClientId expected:% received:%.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), _clientId, response->meClientResponse.clientId);
                    continue;
                }

                if (response->seqNum != _nextExpSeqNum) {
                    _logger.log("%:% %() % ERROR Incorrect sequence number. ClientId:%. SeqNum expected:% received:%.\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), _clientId, _nextExpSeqNum, response->seqNum);
                    continue;
                }

                _nextExpSeqNum++;

                // forwad response to trading engine
                auto nextWrite = _incomingResponses->getNextWriteTo();
                *nextWrite = std::move(response->meClientResponse);
                _incomingResponses->updateWriteIndex();
                
                // remove response from recv buffer
                memcpy(socket->recv_buffer.data(), socket->recv_buffer.data() + i, socket->next_recv_valid_index - i);
                socket->next_recv_valid_index -= i;
            }
        }
    }
}