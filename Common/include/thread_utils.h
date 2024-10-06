#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <iostream>

namespace Common {
    inline auto setThreadCore(int coreId) noexcept {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(coreId, &cpuset);
        return (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0);
    }

    template<typename T, typename ...A>
    inline auto createAndStartThread(int coreId, const std::string &name, T &&func, A &&... args) noexcept {
        auto t = new std::thread([&]() {
            if (coreId >= 0 && !setThreadCore(coreId)) {
                std::cerr << "Failed to set core affinity for " << name << " " << pthread_self() << " to " << coreId << std::endl;
                exit(EXIT_FAILURE);
            }
            std::cerr << "Set core affinity for " << name << " " << pthread_self() << " to " << coreId << std::endl;
    
            std::forward<T>(func)((std::forward<A>(args))...);
        });

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    
        return t;
    }   
}

