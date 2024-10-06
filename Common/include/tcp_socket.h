#pragma once

#include <functional>
#include <stddef.h>
#include <vector>
#include "socket_utils.h"
#include "logging.h"

namespace Common {
    constexpr size_t TCPBufferSize = 64 * 1024 * 1024;    

    struct TCPSocket {
        explicit TCPSocket(Logger &logger): logger(logger) {
            send_buffer.resize(TCPBufferSize);
            recv_buffer.resize(TCPBufferSize);
            recv_callback = [this](auto socket, auto rx_time) {
                defaultRecvCallback(socket, rx_time);
            };
        }

        TCPSocket() = delete;
        TCPSocket(const TCPSocket&) = delete;
        TCPSocket(const TCPSocket&&) = delete;
        TCPSocket& operator=(const TCPSocket&) = delete;
        TCPSocket& operator=(const TCPSocket&&) = delete;

        ~TCPSocket() {
            close(fd);
            fd = -1;
        }

        // method for closing connection of socket
        auto destroy() noexcept -> void;
        // method for connection socket
        auto connect(const std::string& ip, const std::string& iface, int port, bool is_listening) -> int;
        // method for sending data
        auto send(const void* data, size_t len) noexcept -> void;
        // method for sending and receiving data
        auto sendAndRecv() noexcept -> bool;


        int fd = -1;
        std::vector<char> send_buffer;
        size_t next_send_valid_index = 0;
        std::vector<char> recv_buffer;
        size_t next_recv_valid_index = 0;
        bool send_disconnected = false;
        bool recv_disconnected = false;
        struct sockaddr_in inInAddr;
        
        std::function<void(TCPSocket* s, Nanos rx_time)> recv_callback;
        std::string time_str;
        Logger& logger;

        static void defaultRecvCallback(TCPSocket* s, Nanos rx_time) noexcept;
    };
}