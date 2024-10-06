#pragma once

#include <vector>
#include <atomic>
#include <iostream>
#include "macros.h"

namespace Common {
    template<typename T>
    class LFQueue final {
    public:
        LFQueue(size_t num_elements) : _store(num_elements, T()) {}
        LFQueue() = delete;
        LFQueue(const LFQueue&) = delete;
        LFQueue(const LFQueue&&) = delete;
        LFQueue& operator=(const LFQueue&) = delete;
        LFQueue& operator=(const LFQueue&&) = delete;

        auto getNextWriteTo() noexcept {
            return &_store[_next_write_index];
        }

        auto updateWriteIndex() noexcept {
            _next_write_index = (_next_write_index + 1) % _store.size();
            _num_elements++;
        }

        auto getNextRead() noexcept -> const T* {
            return (_next_read_index == _next_write_index)? nullptr : &_store[_next_read_index];
        }

        auto updateReadIndex() noexcept {
            _next_read_index = (_next_read_index + 1) % _store.size();
            ASSERT(_num_elements != 0, "Read an invalid element " + std::to_string(pthread_self()));
            _num_elements--;
        }

        auto size() noexcept {
            return _num_elements.load();
        }

    private:
        std::vector<T> _store;
        std::atomic<size_t> _next_write_index = {0};
        std::atomic<size_t> _next_read_index = {0};
        std::atomic<size_t> _num_elements = {0};
    };
}