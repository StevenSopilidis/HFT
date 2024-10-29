#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <unordered_set>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "macros.h"
#include "logging.h"

namespace Common {
    constexpr int MAX_TCP_SERVER_BACK_LOG = 1024;
    
    inline auto getIfaceIP(const std::string& iface) noexcept -> std::string {
        char buff[NI_MAXHOST] = {'\0'};

        ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) != -1) {
            for (ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr && ifa->ifa_name == iface && ifa->ifa_addr->sa_family == AF_INET) {
                    getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), buff, sizeof(buff), NULL, 0, NI_NUMERICHOST);
                    break;
                }
            }
            freeifaddrs(ifaddr);
        }

        return buff;
    }

    inline auto setNonBlocking(int fd) noexcept -> bool {
        const auto flags = fcntl(fd, F_GETFL, -1);

        if (flags == -1)
            return false;

        if (flags & O_NONBLOCK) // fd is already set to non blocking
            return true;

        return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);
    }

    inline auto setNoDelay(int fd) noexcept -> bool {
        int one = 1;
        return (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void *>(&one), sizeof(one)) != -1);
    }

    inline auto wouldBlock() noexcept -> bool {
        return (errno == EWOULDBLOCK || errno == EINPROGRESS);
    }

    inline auto setTTL(int fd, int ttl) noexcept -> bool {
        return (setsockopt(fd, IPPROTO_TCP, IP_TTL, reinterpret_cast<void *>(&ttl), sizeof(ttl)) != -1);
    }

    inline auto setMcastTTL(int fd, int mcast_ttl) noexcept -> bool {
        return (setsockopt(fd, IPPROTO_TCP, IP_MULTICAST_TTL, reinterpret_cast<void *>(&mcast_ttl), sizeof(mcast_ttl)) != -1);
    }

    inline auto join(int fd, const std::string& ip) -> bool {
        const ip_mreq mreq{{inet_addr(ip.c_str())}, {htonl(INADDR_ANY)}};
        return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != -1);
    }

    inline auto setSOTimestamp(int fd) noexcept -> bool {
        int one = 1;
        return (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, reinterpret_cast<void*>(&one), sizeof(one)) != 1);
    }

    [[nodiscard]] inline auto createSocket(Logger &logger, const std::string& t_ip, const std::string& iface, int port, bool is_udp, bool is_blocking, bool is_listening, int ttl, bool needs_so_timestamp) noexcept -> int {
        std::string time_str;

        const auto ip = t_ip.empty()? getIfaceIP(iface) : t_ip;

        logger.log("%:% %() % ip:% iface:% port:% is_udp:% is_blocking:% is_listening:% ttl:% SO_time:%\n",__FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str), ip, iface, port, is_udp, is_blocking, is_listening, ttl, needs_so_timestamp);

        const int input_flags = (is_listening ? AI_PASSIVE : 0) | (AI_NUMERICHOST | AI_NUMERICSERV);
        const addrinfo hints{input_flags, AF_INET, is_udp ? SOCK_DGRAM : SOCK_STREAM,
            is_udp ? IPPROTO_UDP : IPPROTO_TCP, 0, 0, nullptr, nullptr};

        addrinfo *result = nullptr;

        const auto rc = getaddrinfo(ip.c_str(), std::to_string(port).c_str(), &hints, &result);        
        ASSERT(!rc, "getaddrinfo() failed. error: " + std::string(gai_strerror(rc)) + " errno: " + strerror(errno));

        int fd = -1;
        int one = 1;

        for (addrinfo *rp = result; rp; rp = rp->ai_next)
        {
            ASSERT((fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) != -1, "socket() failed, error: " + std::string(strerror(errno)));
        
            ASSERT(setNonBlocking(fd), "setNonBlocking() failed, error: " + std::string(strerror(errno)));

            if (!is_udp) // disable nagle for tcp sockets
                ASSERT(setNonBlocking(fd), "setNonBlocking() failed, error: " + std::string(strerror(errno)));

            if (!is_listening) // connect to remote addr
                ASSERT(connect(fd, rp->ai_addr, rp->ai_addrlen) != -1, "connect() failed, error: " + std::string(strerror(errno)));
        
            if (is_listening) // allow reuse of addr in call to bind
                ASSERT(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&one), sizeof(one)) == 0, "setsockopt() SO_REUSEADDR failed. errno:" + std::string(strerror(errno)));

            if (is_listening) { // bind to specific port
                const sockaddr_in addr{AF_INET, port, {htonl(INADDR_ANY)}, {}};
                ASSERT(bind(fd, is_udp? reinterpret_cast<const struct sockaddr *>(&addr) : rp->ai_addr, sizeof(addr)) == 0, "bind() failed. errno:%" + std::string(strerror(errno)));
            }

            if(!is_udp && is_listening) // listen for incoming tcp connections
                ASSERT(listen(fd, MAX_TCP_SERVER_BACK_LOG) == 0, "listen() failed, error: " + std::string(strerror(errno)));
        
            if (needs_so_timestamp) // enamble software timestamps
                ASSERT(setSOTimestamp(fd), "setSOTimestamp() failed, error: " + std::string(strerror(errno)));
        }
        return fd;
    }
}