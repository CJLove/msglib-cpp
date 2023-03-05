#pragma once

#include "BytePool.h"
#include "Receiver.h"
#include <memory_resource>
#include <array>

namespace msglib {
using Label = uint16_t;

namespace detail {

/**
 * @brief Default resource pool sizes and capacities
 */
static constexpr size_t LARGE_SIZE = 2048;
static constexpr size_t SMALL_SIZE = 256;
static constexpr size_t LARGE_CAP = 200;
static constexpr size_t SMALL_CAP = 200;

/**
 * @brief Maximum number of mailboxes given a 16-bit label (0..65535)
 */
static constexpr size_t MAX_MAILBOX = 65536;

/**
 * @brief Resources is a struct encapsulating dynamically allocated resources within the
 *        shared Mailbox infrastructure.
 */
struct Resources {
    /**
     * @brief Size of elements in the "small" BytePool
     */
    size_t m_smallSize;

    /**
     * @brief Size of elements in the "large" BytePool
     */
    size_t m_largeSize;

    /**
     * @brief Number of bytes to be allocated for the monotonic buffer resource
     */
    size_t m_byteSize;

    /**
     * @brief Bytes to be used by the monotonic buffer resource
     */
    std::unique_ptr<std::byte[]> m_bytes;

    /**
     * @brief Monotonic buffer resource supporting small and large pools
     *        and the unordered_map of receivers
     */
    std::pmr::monotonic_buffer_resource m_byteResource;

    /**
     * @brief Synchronized pool resource
     */
    std::pmr::synchronized_pool_resource m_syncResource;

    /**
     * @brief BytePool for allocating "small" data blocks
     */
    detail::BytePool m_smallPool;

    /**
     * @brief BytePool for allocating "large" data blocks
     */
    detail::BytePool m_largePool;

    /**
     * @brief Collection of registered receivers indexed by Label
     */
    std::array<Receivers, MAX_MAILBOX> m_mailboxes;

    /**
     * @brief Construct a new Resources object
     * 
     * @param smallSize - size of elements in the "small" BytePool
     * @param smallCap - capacity of the "small" BytePool
     * @param largeSize - size of elements in the "large" BytePool
     * @param largeCap - capacity of the "large" BytePool
     */
    Resources(size_t smallSize, size_t smallCap, size_t largeSize, size_t largeCap)
        : m_smallSize(smallSize)
        , m_largeSize(largeSize)
        , m_byteSize((smallSize * smallCap) + (largeSize * largeCap) + 1000)
        , m_bytes(std::make_unique<std::byte[]>(m_byteSize))
        , m_byteResource(m_bytes.get(), m_byteSize, std::pmr::null_memory_resource())
        , m_syncResource(&m_byteResource)
        , m_smallPool(smallSize, smallCap, &m_syncResource)
        , m_largePool(largeSize, largeCap, &m_syncResource) {
    }

    /**
     * @brief Destroy the Resources object
     */
    ~Resources() 
    {}

};

/**
 * @brief MailboxData represents data shared among all mailbox instances.
 */
class MailboxData {
public:
    MailboxData() noexcept = default;

    bool Initialize() {
        try {
            std::lock_guard<std::mutex> guard(m_mutex);
            if (!m_initialized) {
                m_resources = std::make_unique<Resources>(SMALL_SIZE, SMALL_CAP, LARGE_SIZE, LARGE_CAP);
                m_initialized = true;
            }
            else {
                return false;
            }
        } catch (std::exception &e) {
            return false;
        }
        return true;
    }

    bool Initialize(size_t smallSize, size_t smallCap, size_t largeSize, size_t largeCap) {
        try {
            std::lock_guard<std::mutex> guard(m_mutex);
            if (!m_initialized) {
                m_resources = std::make_unique<Resources>(smallSize, smallCap, largeSize, largeCap);
                m_initialized = true;
            }
            else {
                // Already initialized
                return false;
            }
        } catch (std::exception &e) {
            return false;
        }
        return true;
    }

    /**
     * @brief Register a Mailbox instance as a receiver for a particular label
     */
    bool RegisterForLabel(msglib::Label label, msglib::Mailbox *mbox) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (!m_initialized) {
            Initialize();
        }
        return m_resources->m_mailboxes[label].add(mbox);
    }

    /**
     * @brief Unregister a Mailbox instance as a receiver for a particular label
     */
    bool UnregisterForLabel(msglib::Label label, Mailbox *mbox) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) {
            Initialize();
        }
        m_resources->m_mailboxes[label].remove(mbox);
        return true;
    }

    /**
     * @brief Get the registered receivers for the specified label
     *
     * @return Receivers&
     */
    Receivers &GetReceivers(msglib::Label label) {
        return m_resources->m_mailboxes[label];
    }

    std::mutex &GetMutex() {
        return m_mutex;
    }

    /**
     * @brief Get a small message block from the pool
     *
     * @return SmallMsg* - small message or nullptr
     */
    detail::DataBlock allocateSmall() {
        if (!m_initialized) {
            Initialize();
        }
        try {
            return m_resources->m_smallPool.alloc();
        }
        catch (std::exception &) {
            return detail::DataBlock();
        }
    }

    /**
     * @brief Free a small message block
     *
     */
    void freeSmall(std::byte *msg) {
        if (m_resources) {
            m_resources->m_smallPool.free(msg);
        }
    }

    /**
     * @brief Get a large message block from the pool
     *
     * @return LargeMsg* - large message or nullptr
     */
    detail::DataBlock allocateLarge() {
        if (!m_initialized) {
            Initialize();
        }
        try {
            return m_resources->m_largePool.alloc();
        }
        catch (std::exception &) {
            return detail::DataBlock();
        }
    }

    /**
     * @brief Free a large message block
     *
     * @param msg - msg to be freed
     */
    void freeLarge(std::byte *msg) {
        if (m_resources) {
            m_resources->m_largePool.free(msg);
        }
    }

    size_t smallSize() const {
        if (m_resources) {
            return m_resources->m_smallSize;
        }
        return 0;
    }

    size_t largeSize() const {
        if (m_resources) {
            return m_resources->m_largeSize;
        }
        return 0;
    }

private:
    /**
     * @brief Mutex protecting Mailbox resources
     */
    std::mutex m_mutex;

    /**
     * @brief State information for mailbox registration
     */
    bool m_initialized = false;

    /**
     * @brief Dynamically allocated resources
     */
    std::unique_ptr<Resources> m_resources;

};

}  // namespace detail

}  // namespace msglib