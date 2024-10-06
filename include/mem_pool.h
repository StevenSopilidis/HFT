#pragma once

#include <string>
#include <vector>
#include "macros.h"

namespace Common {
    template<typename T>
    class MemPool final {
        public:
            explicit MemPool(std::size_t numElements) : _store(numElements, {T(), true}) {
                // assert that T object is the first member of ObjectBlock
                ASSERT(reinterpret_cast<const ObjectBlock*>(&(_store[0].object)) == &(_store[0]), "T object should be first member of ObjectBlock");
            }

            MemPool() = delete;
            MemPool(const MemPool& pool) = delete;
            MemPool(const MemPool&& pool) = delete;
            MemPool& operator=(const MemPool&) = delete;
            MemPool& operator=(const MemPool&&) = delete;

            template<typename ...Args>
            T* allocate(Args... args) noexcept {
                auto objectBlock = &(_store[_nextFreeIndex]);
                ASSERT(objectBlock->isFree, "Expected free ObjectBlock at index:" + std::to_string(_nextFreeIndex));

                T* retObject = &(objectBlock->object);
                retObject = new(retObject) T(args...); // use placement new operator to contruct new object to preallocated address
                objectBlock->isFree = false;
                updateNextFreeIndex();
                return retObject;
            }

            auto deallocate(const T* elem) noexcept {
                const auto elemIndex = (reinterpret_cast<const ObjectBlock *>(elem) - &_store[0]);
            
                ASSERT(elemIndex >= 0 && static_cast<size_t>(elemIndex) < _store.size(), "Element being deallocated does not belong to this Memory pool");
                ASSERT(!_store[elemIndex].isFree, "Expected in-use ObjectBlock at index:" + std::to_string (elemIndex)); 
                _store[elemIndex].isFree = true;
            }

        private:
            auto updateNextFreeIndex() noexcept {
                const auto initialFreeIndex = _nextFreeIndex;

                while (!_store[_nextFreeIndex].isFree)
                {
                    ++_nextFreeIndex;

                    if (UNLIKELY(_nextFreeIndex == _store.size())) {
                        _nextFreeIndex = 0;
                    }

                    // store is fully allocated
                    if (UNLIKELY(initialFreeIndex == _nextFreeIndex)) {
                        FATAL("Memory Pool out of space.");
                    }
                }
                
            } 


            struct ObjectBlock {
                T object;
                bool isFree = true;
            };

            std::vector<ObjectBlock> _store;
            size_t _nextFreeIndex = 0;
    };
}