#include "logging.h"

namespace Common {
    void Logger::flushQueue() noexcept {
        while(_running) {
            for (auto next = _queue.getNextRead(); _queue.size() && next; next = _queue.getNextRead()) {
                switch (next->_type)
                {
                case LogType::CHAR:
                    _file << next->_value.c;
                    break;
                case LogType::INTEGER:
                    _file << next->_value.i;
                    break;
                case LogType::LONG_INTEGER:
                    _file << next->_value.l;
                    break;
                case LogType::LONG_LONG_INTEGER:
                    _file << next->_value.ll;
                    break;
                case LogType::UNSIGNED_INTEGER:
                    _file << next->_value.u;
                    break;
                case LogType::UNSIGNED_LONG_INTEGER:
                    _file << next->_value.ul;
                    break;
                case LogType::UNSIGNED_LONG_LONG_INTEGER:
                    _file << next->_value.ull;
                    break;
                case LogType::FLOAT:
                    _file << next->_value.f;
                    break;
                case LogType::DOUBLE: 
                    _file << next->_value.d;
                    break;
                }
                
                _queue.updateReadIndex();
            }
            
            _file.flush();
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }
    }

    void Logger::pushValue(const LogElement& logElement) noexcept {
        *(_queue.getNextWriteTo()) = logElement;

        _queue.updateWriteIndex();
    }

    void Logger::pushValue(const char value) noexcept {
        pushValue(LogElement{LogType::CHAR, {.c = value}});
    }

    void Logger::pushValue(const char* value) noexcept {
        while (*value)
        {
            pushValue(*value);
            value++;
        }
    }

    void Logger::pushValue(const std::string& value) noexcept {
        pushValue(value.c_str());
    }

    void Logger::pushValue(const int value) noexcept {
        pushValue(LogElement{LogType::INTEGER, {.i = value}});
    }

    void Logger::pushValue(const long value) noexcept {
        pushValue(LogElement{LogType::LONG_INTEGER, {.l = value}});
    }

    void Logger::pushValue(const long long value) noexcept {
        pushValue(LogElement{LogType::LONG_LONG_INTEGER, {.ll = value}});
    }

    void Logger::pushValue(const unsigned value) noexcept {
        pushValue(LogElement{LogType::UNSIGNED_INTEGER, {.u = value}});
    }

    void Logger::pushValue(const unsigned long value) noexcept {
        pushValue(LogElement{LogType::UNSIGNED_LONG_INTEGER, {.ul = value}});
    }

    void Logger::pushValue(const unsigned long long value) noexcept {
        pushValue(LogElement{LogType::UNSIGNED_LONG_LONG_INTEGER, {.ull = value}});
    }

    void Logger::pushValue(const float value) noexcept {
        pushValue(LogElement{LogType::FLOAT, {.f = value}});
    }

    void Logger::pushValue(const double value) noexcept {
        pushValue(LogElement{LogType::DOUBLE, {.d = value}});
    }

}