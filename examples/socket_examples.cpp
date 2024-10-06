#include "time_utils.h"
#include "logging.h"
#include "tcp_server.h"

int main(int, char **) {
    using namespace Common;

    std::string time_str_;
    Logger logger("socket_example.log");

    auto tcpServerRecvCallback = [&](TCPSocket *socket, Nanos rx_time) noexcept {
        logger.log("TCPServer::defaultRecvCallback() socket:% len:% rx:%\n",
        socket->fd, socket->next_recv_valid_index, rx_time);

        const std::string reply = "TCPServer received msg:" + std::string(socket->recv_buffer.data(), socket->next_recv_valid_index);
        socket->next_recv_valid_index = 0;

        socket->send(reply.data(), reply.length());
    };

    auto tcpServerRecvFinishedCallback = [&]() noexcept {
        logger.log("TCPServer::defaultRecvFinishedCallback()\n");
    };

    auto tcpClientRecvCallback = [&](TCPSocket *socket, Nanos rx_time) noexcept {
        const std::string recv_msg = std::string(socket->recv_buffer.data(), socket->next_recv_valid_index);
        socket->next_recv_valid_index = 0;

        logger.log("TCPSocket::defaultRecvCallback() socket:% len:% rx:% msg:%\n",
        socket->fd, socket->next_recv_valid_index, rx_time, recv_msg);
    };

    const std::string iface = "lo";
    const std::string ip = "127.0.0.1";
    const int port = 12345;

    

    std::cout << "Server starting" << std::endl;


    TCPServer server(logger);
    server.recv_callback = tcpServerRecvCallback;
    server.recv_finished_callback = tcpServerRecvFinishedCallback;
    
    server.listenAndServe(iface, port);

    return 0;
}