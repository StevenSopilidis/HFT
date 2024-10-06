#include "tcp_server.h"

namespace Common {
    auto TCPServer::defaultRecvFinishedCallback() noexcept -> void{
        logger.log("%:% %() % TCPServer::defaultRecvFinishedCallback()\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
    }

    auto TCPServer::defaultRecvCallback(TCPSocket* s, Nanos rx_time) noexcept -> void {
        logger.log("%:% %() % TCPServer::defaultRecvCallback() socket:% len:% rx:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str), s->fd, s->next_recv_valid_index, rx_time);
    }

    auto TCPServer::destroy() noexcept -> void {
        close(efd);
        efd = -1;
        listener_socket.destroy();
    }

    auto TCPServer::listen(const std::string& iface, const int port) noexcept -> void {
        efd = epoll_create(1);
        ASSERT(efd >= 0, "epoll_create() failed, error: " + std::string(strerror(errno)));
        ASSERT(listener_socket.connect("", iface, port, true) >= 0, "listener_socket.connect() failed, error: " + std::string(strerror(errno)));
        ASSERT(epoll_add(&listener_socket), "epoll_ctrl() failed, error: " + std::string(strerror(errno)));
    }

    auto TCPServer::epoll_add(TCPSocket* s) noexcept -> bool {
        epoll_event ev{EPOLLET | EPOLLIN, {reinterpret_cast<void *>(s)}};
        return !epoll_ctl(efd, EPOLL_CTL_ADD, s->fd, &ev);
    }

    auto TCPServer::epoll_rmv(TCPSocket* s) noexcept -> bool {
        return (epoll_ctl(efd, EPOLL_CTL_DEL, s->fd, nullptr) != -1);
    }  

    auto TCPServer::rmv(TCPSocket* s) noexcept -> void {
        epoll_rmv(s);
        sockets.erase(std::remove(sockets.begin(), sockets.end(), s), sockets.end());
        receive_sockets.erase(std::remove(receive_sockets.begin(), receive_sockets.end(), s), receive_sockets.end());
        send_sockets.erase(std::remove(send_sockets.begin(), receive_sockets.end(), s), send_sockets.end());
    
    }

    auto TCPServer::poll() noexcept -> void {
        const int max_events = 1 + sockets.size();
        for (auto socket : disconnect_sockets) {
            rmv(socket);
        }

        const int n = epoll_wait(efd, events, max_events, 0);

        bool have_new_connection = false;
        for (int i = 0; i < n; i++)
        {
            epoll_event event = events[i];
            auto socket = reinterpret_cast<TCPSocket *>(event.data.ptr);

            // check if we have read from socket
            if (event.events & EPOLLIN) {
                // if its a listener socket we have new connection
                if (socket == &listener_socket) {
                    logger.log("%:% %() % EPOLLIN istener_socket:%\n", __FILE__, __LINE__, __FUNCTION__,Common::getCurrentTimeStr(&time_str),socket->fd);
                    have_new_connection = true;
                    continue;
                }   

                // we have data to read from client socket
                logger.log("%:% %() % EPOLLIN socket:%\n",__FILE__, __LINE__, __FUNCTION__,Common::getCurrentTimeStr(&time_str), socket->fd);
                if(std::find(receive_sockets.begin(), receive_sockets.end(), socket) == receive_sockets.end())
                    receive_sockets.push_back(socket);
            } 

            // check if we can write to socket
            if (event.events & EPOLLOUT) {
                logger.log("%:% %() % EPOLLOUT socket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str), socket->fd);
                if(std::find(send_sockets.begin(), send_sockets.end(), socket) == send_sockets.end())
                    receive_sockets.push_back(socket);
            }

            // check if there was an error or connection was closed
            if (event.events & (EPOLLERR | EPOLLHUP)) {
                logger.log("%:% %() % EPOLLERR socket:%\n", __FILE__, __LINE__, __FUNCTION__,Common::getCurrentTimeStr(&time_str), socket->fd); 
                if(std::find(disconnect_sockets.begin(), disconnect_sockets.end(), socket) == disconnect_sockets.end())
                    disconnect_sockets.push_back(socket);
            }
        }

        while (have_new_connection)
        {
            logger.log("%:% %() % have_new_connection\n",__FILE__, __LINE__, __FUNCTION__,Common::getCurrentTimeStr(&time_str));
            sockaddr_storage addr;
            socklen_t addr_len = sizeof(addr);

            int fd = accept(listener_socket.fd, reinterpret_cast<sockaddr*>(&addr), &addr_len);
            if (fd == -1)
                break;
            
            ASSERT(setNonBlocking(fd) && setNoDelay(fd), "Failed to set non-blocking or no-delay on socket:" + std::to_string(fd));
            
            logger.log("%:% %() % accepted socket:%\n", __FILE__, __LINE__, __FUNCTION__,Common::getCurrentTimeStr(&time_str), fd);
        
            TCPSocket* socket = new TCPSocket(logger);
            socket->fd = fd;
            socket->recv_callback = recv_callback;

            ASSERT(epoll_add(socket), "Unable to add socket: " + std::string(strerror(errno)));
            
            if(std::find(sockets.begin(), sockets.end(), socket) == sockets.end())
                sockets.push_back(socket);
            if(std::find(receive_sockets.begin(), receive_sockets.end(), socket) == receive_sockets.end())
                receive_sockets.push_back(socket);
        }
    }

    auto TCPServer::sendAndRecv() noexcept -> void {
        auto recv = false;

        std::for_each(receive_sockets.begin(), receive_sockets.end(), [&recv](TCPSocket* socket){
            recv |= socket->sendAndRecv();
        });

        if (recv)
            recv_finished_callback();

        std::for_each(send_sockets.begin(), send_sockets.end(), [](TCPSocket* socket){
            socket->sendAndRecv();
        });
    }

    auto TCPServer::listenAndServe(const std::string& iface, int port) noexcept -> void {
        listen(iface, port);

        while(true) {
            poll();
            sendAndRecv();
        }   
    }


}
