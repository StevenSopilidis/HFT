#pragma once

#include <string>
#include <fstream>
#include <cstdio>
#include "macros.h"
#include "lf_queue.h"
#include "thread_utils.h"
#include "time_utils.h"

namespace Common {
    constexpr size_t LOG_QUEUE_IZE = 8 * 1024 * 1024;

    enum class LogType: uint8_t {
        CHAR=0,
        INTEGER=1,
        LONG_INTEGER=2,
        LONG_LONG_INTEGER=3,
        UNSIGNED_INTEGER=4,
        UNSIGNED_LONG_INTEGER=5,
        UNSIGNED_LONG_LONG_INTEGER=6,
        FLOAT=7,
        DOUBLE=8,
    };

    struct LogElement {
        // type of log element
        LogType _type = LogType::CHAR;
        // value of actual element
        union {
            char c;
            int i; long l; long long ll;
            unsigned u; unsigned long ul; unsigned long long ull;
            float f; double d;
        } _value;
    };

    class Logger final {
    public:
        explicit Logger(const std::string& filename) 
        : _filename(filename), _queue(LOG_QUEUE_IZE) {
            _file.open(_filename);
            ASSERT(_file.is_open(), "Could not open log file: " + _filename);
            _logger_thread = createAndStartThread(-1, "Common/Logger", [this](){ flushQueue(); });
            ASSERT(_logger_thread != nullptr, "Failed to start logger thread");

        }

        ~Logger() {
            std::string time_str;

            std::cerr << Common::getCurrentTimeStr(&time_str) << " Flushing and closing Logger for " << _filename << std::endl;
            while (_queue.size())
            {
                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(1s);
            }

            _running = false;
            _logger_thread->join();
            _file.close();

            std::cerr << Common::getCurrentTimeStr(&time_str) << " Logger for " << _filename << " exiting." << std::endl;            
        }

        Logger() = default;
        Logger(const Logger&) = delete;
        Logger(const Logger&&) = delete;
        Logger operator=(const Logger&) = delete;
        Logger operator=(const Logger&&) = delete;


        // function for pulling from queue and flushing to log file
        void flushQueue() noexcept;

        // functions for pushing to the queue
        void pushValue(const LogElement& logElement) noexcept;
        void pushValue(const char value) noexcept;
        void pushValue(const char* value) noexcept;
        void pushValue(const std::string& value) noexcept;
        void pushValue(const int value) noexcept;
        void pushValue(const long value) noexcept;
        void pushValue(const long long value) noexcept;
        void pushValue(const unsigned value) noexcept;
        void pushValue(const unsigned long value) noexcept;
        void pushValue(const unsigned long long value) noexcept;
        void pushValue(const float value) noexcept;
        void pushValue(const double value) noexcept;

        // function for logging to the file
        template <typename T, typename... A>
        auto log(const char* s, const T& value, A... args) noexcept {
            while (*s)
            {
                if (*s == '%') {
                    auto nextChar = *(s+1);
                    if (UNLIKELY(nextChar == '%')) {
                        ++s;
                    } else {
                        pushValue(value);
                        log(s+1, args...);  
                        return;
                    }
                }
                pushValue(*s++);
            }
            
            FATAL("extra arguments provided to log()");
        }

        void log(const char* s) noexcept {
            while (*s) {
                if (*s == '%' && *(s + 1) == '%') {
                    ++s;  // skip the escaped %
                }
                
                pushValue(*s++);  // process the remaining format string
            }
        }

    private:
        const std::string _filename;
        std::ofstream _file;
        LFQueue<LogElement> _queue;
        std::atomic<bool> _running = {true};
        std::thread* _logger_thread = nullptr;
    };
}