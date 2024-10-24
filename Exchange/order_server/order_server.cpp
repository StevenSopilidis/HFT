#include "order_server.h"


namespace Exchange {
    OrderServer::OrderServer(ClientRequestLFQueue* clientRequests, ClientResponseLFQueue* clientResponses, const std::string& iface, int port) 
    : _iface(iface), _port(port), _outgoingResponses(clientResponses), _logger("exchange_order_server.log"), _server(_logger), _fifoSequencer(clientRequests, &_logger) {
        _cidNextExpSeqNum.fill(1);
        _cidNextOutgoingSeqNum.fill(1);
        _cidTcpSocket.fill(nullptr);
        _server.recv_callback = [this](auto socket, auto rx_time) {
            recvCallback(socket, rx_time);
        };
        _server.recv_finished_callback = [this]() {
            recvFinishedCallback();
        };
    }

    OrderServer::~OrderServer() {
        stop();
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }

    auto OrderServer::start() noexcept -> void {
        _run = true;
        _server.listen(_iface, _port);
        ASSERT(Common::createAndStartThread(-1, "Exchange/OrderServer", [this](){run();}) != nullptr, "Failed to start OrderServer thread.");
    }

    auto OrderServer::run() noexcept -> void {
        _logger.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr));
        while(_run) {
            _server.poll();
            _server.sendAndRecv();

            for (auto clientResponse = _outgoingResponses->getNextRead(); _outgoingResponses->size() && clientResponse; clientResponse = _outgoingResponses->getNextRead()) {
                auto& nextOutgoingSeqNum = _cidNextOutgoingSeqNum[clientResponse->clientId];
                _logger.log("%:% %() % Processing cid:% seq:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), clientResponse->clientId, nextOutgoingSeqNum, clientResponse->toString());

                // make sure that the client socket exists
                    ASSERT(_cidTcpSocket[clientResponse->clientId] != nullptr, "Dont have a TCPSocket for ClientId:" + std::to_string(clientResponse->clientId));

                _cidTcpSocket[clientResponse->clientId]->send(&nextOutgoingSeqNum, sizeof(nextOutgoingSeqNum));
                _cidTcpSocket[clientResponse->clientId]->send(clientResponse, sizeof(clientResponse));

                _outgoingResponses->updateReadIndex();
                nextOutgoingSeqNum++;
            }
        }
    }

    auto OrderServer::stop() noexcept -> void {
        _run = false;
    }

    auto OrderServer::recvCallback(TCPSocket *socket, Nanos rxTime) noexcept -> void {
        _logger.log("%:% %() % Received socket:% len:% rx:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), socket->fd, socket->next_recv_valid_index, rxTime);
        
        if (socket->next_recv_valid_index >= sizeof(OMClientRequest)) {
            size_t i = 0;
            // loop through all the requests that client has sent
            for (; i + sizeof(OMClientRequest) <= socket->next_recv_valid_index; i += sizeof(OMClientRequest)) {
                auto request = reinterpret_cast<const OMClientRequest*>(socket->recv_buffer.data() + i);
                _logger.log("%:% %() % Received %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), request->toString());
            
                auto clientSocket = _cidTcpSocket[request->meClientRequest.clientId];
                // check if this is client's first request
                if (UNLIKELY(clientSocket == nullptr)) {
                    _cidTcpSocket[request->meClientRequest.clientId] = socket;
                }

                // check that client has sent request from same socket
                if(clientSocket!= socket) {
                    _logger.log("%:% %() % Received ClientRequest from ClientId:% on different socket:% expected:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), request->meClientRequest.clientId, socket->fd, _cidTcpSocket[request->meClientRequest.clientId]->fd);
                    continue;
                }

                // check that sequence number sent equals expected sequence number
                auto nextExpectedSeqNum = _cidNextExpSeqNum[request->meClientRequest.clientId];
                if(nextExpectedSeqNum != request->seqNum) {
                    _logger.log("%:% %() % Incorrect sequence number. ClientId:% SeqNum expected:% received:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&_timeStr), request->meClientRequest.clientId, nextExpectedSeqNum, request->seqNum);
                    continue;
                }

                nextExpectedSeqNum++;
                _fifoSequencer.addClientRequest(rxTime, request->meClientRequest);
            }

            memcpy(socket->recv_buffer.data(), socket->recv_buffer.data() + i, socket->next_recv_valid_index - i);
            socket->next_recv_valid_index -= i;
        }
    }

    auto OrderServer::recvFinishedCallback() noexcept -> void {
        _fifoSequencer.sequenceAndPublish();
    }

}
