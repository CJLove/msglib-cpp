#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace msglib {
class Mailbox;

namespace detail {
static constexpr size_t MAX_RECEIVERS = 3;
/**
 * @brief Receivers is a struct holding up to X (default 3) mailbox receivers for a particular event label
 */
struct Receivers {
    std::array<Mailbox *, MAX_RECEIVERS> m_receivers;

    /**
     * @brief Construct a new Receivers object
     */
    Receivers() : m_receivers({nullptr, nullptr, nullptr}) {
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
        for (size_t i = 0; i < MAX_RECEIVERS; i++) {
            if (m_receivers[i] == mbox) {
                m_receivers[i] = nullptr;
            }
            // If no mailboxes remain for this label then it should be removed
            remove &= (m_receivers[i] == nullptr);
        }
        return remove;
    }
};

}  // namespace detail
}  // namespace msglib