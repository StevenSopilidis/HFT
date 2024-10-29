#pragma once

#include <functional>
#include "logging.h"
#include "socket_utils.h"

namespace Common {
    // max size of send and recv buffers of mcast_socket
    constexpr size_t McastSocketBufferSize = 64 * 1024 * 1024;

    struct McastSocket {
        McastSocket(Logger &logger): logger(logger) {
            send_buffer.resize(McastSocketBufferSize);
            recv_buffer.resize(McastSocketBufferSize);
        }

        // initializes multicast socket to send or recv from stream
        // Does not join the multicast stream yet
        auto init(const std::string &ip, const std::string &iface,const int port, bool is_listening) noexcept -> int;

        // add subscription to a multicast stream.
        auto join(const std::string &ip) -> bool;

        // removes subscribtion from multicast stream
        auto leave(const std::string &, int) -> void;

        // publishes data and reads incoming data
        auto sendAndRecv() noexcept -> bool;

        // copies data to send buffer does not publish message
        auto send(const void *data, size_t len) noexcept -> void;

        int socketFd = -1;
        std::vector<char> send_buffer;
        size_t next_send_valid_index;
        std::vector<char> recv_buffer;
        size_t next_recv_valid_index;

        std::function<void(McastSocket*)> recv_callback = nullptr;
        Logger& logger;        
        std::string timeStr;
    };
}