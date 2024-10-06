#include "tcp_socket.h"

namespace Common {
    void TCPSocket::defaultRecvCallback(TCPSocket* s, Nanos rx_time) noexcept {
        s->logger.log("%:% %() %TCPSocket::defaultRecvCallback() socket:% len:% rx:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&s->time_str), s->fd, s->next_recv_valid_index, rx_time);
    }

    auto TCPSocket::destroy() noexcept -> void {
        close(fd);
        fd = -1;
    }

    auto TCPSocket::connect(const std::string& ip, const std::string& iface, int port, bool is_listening) -> int {
        destroy();
        fd = createSocket(logger, ip, iface, port, false, false, is_listening, 0, true);
        
        inInAddr.sin_addr.s_addr = INADDR_ANY;
        inInAddr.sin_port = htons(port);
        inInAddr.sin_family = AF_INET;
        return fd;
    }

    auto TCPSocket::send(const void* data, size_t len) noexcept -> void {
        mempcpy(send_buffer.data() + next_send_valid_index, data, len);
        next_send_valid_index += len;
    }

    auto TCPSocket::sendAndRecv() noexcept -> bool {
        char ctrl[CMSG_SPACE(sizeof(struct timeval))];
        auto cmsg = reinterpret_cast<struct cmsghdr*>(&ctrl);

        struct iovec iov;
        iov.iov_base = recv_buffer.data() + next_recv_valid_index;
        iov.iov_len = TCPBufferSize - next_recv_valid_index;
        
        msghdr msg;
        msg.msg_control = ctrl;
        msg.msg_controllen = sizeof(ctrl);
        msg.msg_name = &inInAddr;
        msg.msg_namelen = sizeof(inInAddr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        
        // Non-blocking call to read available data.
        const auto read_size = recvmsg(fd, &msg, MSG_DONTWAIT);
        if (read_size > 0) {
            next_recv_valid_index += read_size;

            Nanos kernel_time = 0;
            timeval time_kernel;
            if (cmsg->cmsg_level == SOL_SOCKET &&
                cmsg->cmsg_type == SCM_TIMESTAMP &&
                cmsg->cmsg_len == CMSG_LEN(sizeof(time_kernel))) {
                memcpy(&time_kernel, CMSG_DATA(cmsg), sizeof(time_kernel));
                kernel_time = time_kernel.tv_sec * NANOS_TO_SEC + time_kernel.tv_usec * NANOS_TO_MICROS; // convert timestamp to nanoseconds.
            }

            const auto user_time = getCurrentNanos();

            logger.log("%:% %() % read socket:% len:% utime:% ktime:% diff:%\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::getCurrentTimeStr(&time_str), fd, next_recv_valid_index, user_time, kernel_time, (user_time - kernel_time));
            recv_callback(this, kernel_time);
        }

        if (next_send_valid_index > 0) {
            // Non-blocking call to send data.
            const auto n = ::send(fd, send_buffer.data(), next_send_valid_index, MSG_DONTWAIT | MSG_NOSIGNAL);
            logger.log("%:% %() % send socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str), fd, n);
        }
        next_send_valid_index = 0;

        return (read_size > 0);
    }

}