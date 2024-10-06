#pragma once

#include "tcp_socket.h"

namespace Common {
    struct TCPServer {
        explicit TCPServer(Logger& logger) : logger(logger), listener_socket(logger) {
            
        }

        TCPServer() = delete;
        TCPServer(const TCPServer&) = delete;
        TCPServer(const TCPServer&&) = delete;
        TCPServer& operator=(const TCPServer&) = delete;
        TCPServer& operator=(const TCPServer&&) = delete;

        // default function to be called when data is available for read
        auto defaultRecvCallback(TCPSocket* s, Nanos rx_time) noexcept -> void;
        // default function to be called when reading finished
        auto defaultRecvFinishedCallback() noexcept -> void;
        // function for getting server up and running
        auto listenAndServe(const std::string& iface, int port) noexcept -> void;
        
        
    private:
        auto destroy() noexcept -> void;
        // function for adding socket to epoll 
        auto epoll_add(TCPSocket* s) noexcept -> bool;
        // function for removing socket from epoll 
        auto epoll_rmv(TCPSocket* s) noexcept -> bool;  
        // function for removing socket from list of sockets
        auto rmv(TCPSocket* s) noexcept -> void;
        // function for send & receiving from socket
        auto sendAndRecv() noexcept -> void;
        // function for starting polling
        auto poll() noexcept -> void;
        // function for created and starting listening socket
        auto listen(const std::string& iface, const int port) noexcept -> void;

    public:
        int efd = -1;
        TCPSocket listener_socket;
        epoll_event events[1024];
        std::vector<TCPSocket*> sockets, send_sockets, receive_sockets, disconnect_sockets;
        // function to be called when data is available
        std::function<void(TCPSocket *s, Nanos rx_time)> recv_callback;
        // function to be called when we finished reading the data;
        std::function<void()> recv_finished_callback;
        std::string time_str;
        Logger& logger;
    };
}
