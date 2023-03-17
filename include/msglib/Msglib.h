#pragma once

#include "Mailbox.h"
#include "TimerManager.h"

namespace msglib {

/**
 * @brief Initialize timer and mailbox internals with the specified capacities
 *
 * @param smallSize - max size of elements in the "small" pool
 * @param smallCap - capacity of the "small" pool
 * @param largeSize - max size of elements in the "large" pool
 * @param largeCap - capacity of the "large" pool
 * @return true - success
 * @return false - failure
 */
inline bool Initialize(size_t smallSize, size_t smallCap, size_t largeSize, size_t largeCap) {
    auto result = TimerManager::Initialize();
    result &= Mailbox::Initialize(smallSize, smallCap, largeSize, largeCap);
    return result;
}

/**
 * @brief Initialize timer and mailbox internals
 * 
 * @return true - success
 * @return false - failure
 */
inline bool Initialize() {
    auto result = TimerManager::Initialize();
    result &= Mailbox::Initialize();
    return result;
}

}  // namespace msglib