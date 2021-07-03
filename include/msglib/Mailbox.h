#pragma once
#include "Pool.h"
#include "Queue.h"
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

/**
 * @brief DataBlockBase provides a common interface for all instantiations of the DataBlock template class
 */
class DataBlockBase {
public:
    virtual void *get() = 0;
};

/**
 * @brief DataBlock is a templated class providing a class capable of storing any message struct up to a specific size.
 * 
 * @tparam msgSize 
 */
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

/**
 * @brief Message is a representation of any message sent or received via a mailbox.
 */
struct Message {
public:
    /**
     * @brief Construct a new Message object
     */
    Message() = default;

    /**
     * @brief Construct a new Message object with a specific label and no data
     * 
     * @param label - message's label
     */
    Message(Label label) : m_label(label), m_size(0), m_data(nullptr) { }

    /**
     * @brief Construct a new Message object with a specific label and amount of message data
     * 
     * @param label - message's label
     * @param size - message's size
     * @param data - message's data
     */
    Message(Label label, uint16_t size, DataBlockBase *data) : m_label(label), m_size(size), m_data(data) { }

    /**
     * @brief Return this message instance's data as a pointer to an object of type T
     *
     * @tparam T - a POD type
     * @return T - result of the conversion, or nullptr for a size mismatch or invalid type
     */
    template <typename T>
    T *as() {
        if (m_data != nullptr && sizeof(T) == m_size && std::is_trivial<T>()) {
            return static_cast<T *>(m_data->get());
        }
        return nullptr;
    }

    Label m_label = 0;
    uint16_t m_size = 0;
    DataBlockBase *m_data = nullptr;
};

/**
 * @brief Forward declaration of Mailbox class
 */
class Mailbox;

/**
 * @brief Receivers is a struct holding up to X (default 3) mailbox receivers for a particular event label
 */
struct Receivers {
    std::vector<Mailbox *> m_receivers;
    const int MAX_RECEIVERS = 3;

    /**
     * @brief Construct a new Receivers object
     */
    Receivers() {
        m_receivers.resize(MAX_RECEIVERS);
        m_receivers[0] = m_receivers[1] = m_receivers[2] = nullptr;
    }

    /**
     * @brief Construct a new Receivers object and add the first receiver
     *
     * @param mbox - first receiver
     */
    Receivers(Mailbox *mbox) {
        m_receivers.resize(MAX_RECEIVERS);
        m_receivers[0] = mbox;
        m_receivers[1] = m_receivers[2] = nullptr;
    }

    Receivers &operator=(const Receivers &rhs) {
        if (&rhs != this) {
            m_receivers = rhs.m_receivers;
        }
        return *this;
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
        for (int i = 0; i < MAX_RECEIVERS; i++) {
            if (m_receivers[i] == mbox) {
                m_receivers[i] = nullptr;
            }
            // If no mailboxes remain for this label then it should be removed
            remove &= (m_receivers[i] == nullptr);
        }
        return remove;
    }
};

/**
 * @brief MailboxData represents data shared among all mailbox instances.
 */
class MailboxData {
public:
    using SmallBlock = DataBlock<SMALL_SIZE>;
    using LargeBlock = DataBlock<LARGE_SIZE>;

    MailboxData() : m_smallPool(SMALL_CAP), m_largePool(LARGE_CAP) { }

    /**
     * @brief Register a Mailbox instance as a receiver for a particular label
     */
    void RegisterForLabel(Label, Mailbox *);

    /**
     * @brief Unregister a Mailbox instance as a receiver for a particular label
     */
    void UnregisterForLabel(Label, Mailbox *);

    /**
     * @brief Get the registered receivers for the specified label
     * 
     * @return Receivers& 
     */
    Receivers &GetReceivers(Label);

    std::mutex &GetMutex() { return m_mutex; }

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

    /**
     * @brief Free a large message block
     * 
     * @param msg - msg to be freed
     */
    void freeLarge(LargeBlock *msg);

private:
    using Mailboxes = std::unordered_map<Label, Receivers>;

    /**
     * @brief State information for mailbox registration
     */
    std::mutex m_mutex;
    Mailboxes m_mailboxes;

    /**
     * @brief Pools of small and large message blocks
     */
    Pool<SmallBlock> m_smallPool;
    Pool<LargeBlock> m_largePool;

    /**
     * @brief Empty set of receivers, used for unknown labels
     */
    Receivers m_receivers;
};

/**
 * @brief Mailbox provides interfaces for sending and receiving messages to one or more subscribers
 */
class Mailbox {
public:
    /**
     * @brief Construct a new Mailbox object
     */
    Mailbox();

    /**
     * @brief Disable copy construction
     */
    Mailbox(const Mailbox &) = delete;

    /**
     * @brief Disable move construction
     */
    Mailbox(Mailbox &&) = delete;

    /**
     * @brief Destroy the Mailbox object
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

    /**
     * @brief Release the data block associated with a message
     */
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
        std::lock_guard<std::mutex> guard(s_mailboxData->GetMutex());
        
        const auto &receivers = s_mailboxData->GetReceivers(label);
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
     * @param label - signal's label
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
    /**
     * @brief Shared mailbox state among all Mailbox instances
     */
    static std::unique_ptr<MailboxData> s_mailboxData;

    /**
     * @brief Queue for this instance of the Mailbox class
     */
    Queue<Message> m_queue;
};

/**
 * @brief MessageGuard is an RAII-style wrapper class to reclaim resources associated with a Message
 *        which has been received from a mailbox.
 */
class MessageGuard {
public:
    /**
     * Disable default construction
     */
    MessageGuard() = delete;

    /**
     * @brief Construct a new MessageGuard object for a specific mailbox and message
     * 
     * @param mailbox 
     * @param msg 
     */
    MessageGuard(Mailbox &mailbox, Message &msg) : m_mailbox(mailbox), m_msg(msg) { }

    /**
     * @brief Free any datablock associated with this message
     * 
     */
    ~MessageGuard() {
        if (m_msg.m_data != nullptr) {
            m_mailbox.ReleaseMessage(m_msg);
        }
    }

private:
    /**
     * @brief References to the specific mailbox and message
     */
    Mailbox &m_mailbox;
    Message &m_msg;
};

}  // Namespace msglib
