#pragma once
#include "BlockingCollection.h"
#include "Pool.h"
#include <condition_variable>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string.h>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace msglib {

using Label = uint16_t;
static const Label LARGE_SIZE = 2048;
static const Label SMALL_SIZE = 256;
static const Label LARGE_CAP = 200;
static const Label SMALL_CAP = 200;

class DataBlockBase {
public:
    virtual void *get() = 0;
};

struct Message {
public:
    /**
     * @brief Construct a new Message object
     *
     */
    Message() = default;

    Message(Label label) : m_label(label), m_size(0), m_data(nullptr) {}

    Message(Label label, uint16_t size, DataBlockBase *data) : m_label(label), m_size(size), m_data(data) {}

    /**
     * @brief Convert this Message instance to an object of type T
     *
     * @tparam T - a POD type
     * @return T - result of the conversion
     */
    template <typename T>
    T *as() {
        if (m_data != nullptr && sizeof(T) == m_size && std::is_trivial<T>()) {
            // T result;
            // memcpy(&result,m_data->get(),sizeof(T));
            // return result;
            return reinterpret_cast<T *>(m_data->get());
        }
        return nullptr;
    }

    Label m_label = 0;
    uint16_t m_size = 0;
    DataBlockBase *m_data = nullptr;
};

template <size_t msgSize>
class DataBlock : public DataBlockBase {
public:
    /**
     * @brief Construct a new DataBlock object
     *
     */
    DataBlock() : m_size(msgSize) { memset(&m_data[0], 0, sizeof(m_data)); }

    void *get() override { return &m_data[0]; }

    /**
     * @brief Store data into this DataBlock instance from an object of type T
     *
     * @tparam T - a POD type
     * @param t - source of message data
     */
    template <typename T>
    void put(const T &t) {
        if (sizeof(T) <= sizeof(m_data) && std::is_trivial<T>()) {
            memcpy(&m_data[0], &t, sizeof(T));
            m_size = sizeof(T);
        } else {
            throw std::runtime_error("Invalid type for put()");
        }
    }

    size_t size() const { return m_size; }

private:
    uint16_t m_size;
    char m_data[msgSize];
};

class Mailbox;

/**
 * @brief Receivers is a struct holding up to X (default 3) receivers for a particular event label
 *
 */
struct Receivers {
    std::vector<Mailbox *> m_receivers;

    /**
     * @brief Construct a new Receivers object
     *
     */
    Receivers() {
        m_receivers.resize(3);
        m_receivers[0] = m_receivers[1] = m_receivers[2] = nullptr;
    }

    /**
     * @brief Construct a new Receivers object and add the first receiver
     *
     * @param mbox - first receiver
     */
    Receivers(Mailbox *mbox) {
        m_receivers.resize(3);
        m_receivers[0] = mbox;
    }

    /**
     * @brief Add a receiver for this label
     *
     * @param mbox - receiver to be added
     * @return true - receiver was added successfully
     * @return false - receiver was not added (capacity reached)
     */
    bool add(Mailbox *mbox) {
        for (auto &r : m_receivers) {
            if (r == nullptr) {
                r = mbox;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Remove a receiver for this label
     *
     * @param mbox - receiver to be removed
     * @return true - this was the last receiver for this label
     * @return false - one or more receivers remain for this label
     */
    bool remove(Mailbox *mbox) {
        bool remove = true;
        for (auto &r : m_receivers) {
            if (r == mbox) {
                r = nullptr;
            }
            // If no mailboxes remain for this label then it should be removed
            remove &= (r == nullptr);
        }
        return remove;
    }
};

struct MailboxData {
    using Mailboxes = std::unordered_map<Label, Receivers>;
    using SmallBlock = DataBlock<SMALL_SIZE>;
    using LargeBlock = DataBlock<LARGE_SIZE>;

    /**
     * @brief State information for mailbox registration
     *
     */
    std::mutex m_mutex;
    Mailboxes m_mailboxes;

    Pool<SmallBlock> m_smallPool;
    Pool<LargeBlock> m_largePool;
    Receivers m_receivers;

    MailboxData() : m_smallPool(SMALL_CAP), m_largePool(LARGE_CAP) {}

    /**
     * @brief Register a Mailbox instance as a receiver for a particular label
     *
     */
    void RegisterForLabel(Label, Mailbox *);

    /**
     * @brief Unregister a Mailbox instance as a receiver for a particular label
     *
     */
    void UnregisterForLabel(Label, Mailbox *);

    Receivers &GetReceivers(Label);

    /**
     * @brief Get a small message block from the pool
     *
     * @return SmallMsg* - small message or nullptr
     */
    SmallBlock *getSmall();

    /**
     * @brief Free a small message block
     *
     */
    void freeSmall(SmallBlock *msg);

    /**
     * @brief Get a large message block from the pool
     *
     * @return LargeMsg* - large message or nullptr
     */
    LargeBlock *getLarge();

    void freeLarge(LargeBlock *msg);
};

class Mailbox {
public:
    /**
     * @brief Construct a new Mailbox object
     *
     */
    Mailbox();

    /**
     * @brief Disable copy construction
     *
     */
    Mailbox(const Mailbox &) = delete;

    /**
     * @brief Disable move construction
     *
     */
    Mailbox(Mailbox &&) = delete;

    /**
     * @brief Destroy the Mailbox object
     *
     */
    ~Mailbox();

    /**
     * @brief Register to receive messages with this label
     *
     * @param label - message label to register
     */
    void RegisterForLabel(Label label);

    /**
     * @brief Cancel registration to receive messages with this label
     *
     * @param label
     */
    void UnregisterForLabel(Label label);

    void ReleaseMessage(Message &);

    /**
     * @brief Send a message with a specific label and associated data of type T
     *
     * @tparam T - a POD type
     * @param label - the message label
     * @param t - an instance
     */
    template <typename T>
    void SendMessage(Label label, const T &t) {
        const auto receivers = s_mailboxData->GetReceivers(label);
        for (const auto &receiver : receivers.m_receivers) {
            if (receiver == nullptr)
                break;
            if (sizeof(T) < SMALL_SIZE) {
                MailboxData::SmallBlock *m = s_mailboxData->getSmall();
                if (m) {
                    m->put(t);
                    receiver->m_queue.emplace(label, sizeof(T), m);
                } else {
                    throw std::runtime_error("Couldn't get a block");
                }
            } else if (sizeof(T) < LARGE_SIZE) {
                MailboxData::LargeBlock *m = s_mailboxData->getLarge();
                if (m) {
                    m->put(t);
                    receiver->m_queue.emplace(label, sizeof(T), m);
                } else {
                    throw std::runtime_error("Couldn't get a block");
                }
            } else {
                throw std::runtime_error("Message too large");
            }
        }
    }

    /**
     * @brief Send a signal with a specific label
     *
     * @param label - signal label
     */
    void SendSignal(Label label);

    /**
     * @brief Block and wait until a signal/message of a register type is received
     *
     * @param label - label of the signal/message which was received
     * @param msg - associated message data, or nullptr for signal
     *              Note: message is owned by the mailbox
     */
    void Receive(Message &msg);

private:
    // Static mailbox state
    static std::unique_ptr<MailboxData> s_mailboxData;

    code_machina::BlockingCollection<Message> m_queue;
};

}  // Namespace msglib