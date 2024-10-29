#include "mcast_socket.h"

namespace Common {
    auto McastSocket::init(const std::string &ip, const std::string &iface,const int port, bool is_listening) noexcept -> int {
        socketFd = createSocket(logger, ip, iface, port, true, false, is_listening, 0, false);
        return socketFd;
    }

    auto McastSocket::join(const std::string &ip) -> bool {
        return Common::join(socketFd, ip);
    }

    auto McastSocket::leave(const std::string &, int) -> void {
        close(socketFd);    
        socketFd = -1;
    }

    auto McastSocket::sendAndRecv() noexcept -> bool {
        // recv data 
        const ssize_t nRecv = recv(socketFd, recv_buffer.data() + next_recv_valid_index, McastSocketBufferSize - next_recv_valid_index, MSG_DONTWAIT);

        if (nRecv > 0) {
            next_recv_valid_index += nRecv;
            logger.log("%:% %() % read socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&timeStr), socketFd, next_recv_valid_index);
            recv_callback(this);
        }

        // send data
        if (next_send_valid_index > 0) {
            const ssize_t n = ::send(socketFd, send_buffer.data(), next_send_valid_index, MSG_DONTWAIT | MSG_NOSIGNAL);
            logger.log("%:% %() % send socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&timeStr), socketFd, n); 
        }

        next_send_valid_index = 0;
        next_recv_valid_index = 0;
        return (nRecv > 0);
    }

    auto McastSocket::send(const void *data, size_t len) noexcept -> void {
        memcpy(send_buffer.data() + next_send_valid_index, data, len);
        next_send_valid_index += len;
        ASSERT(next_send_valid_index < McastSocketBufferSize, "Mcast socket buffer filled up and sendAndRecv() not called.");
    }

}