#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>


namespace msglib {

using Label = uint16_t;

/**
 * @brief Representation of a message returned from a Mailbox Receive() call
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
    explicit Message(Label label) : m_label(label) {
    }               
                    
    /**         
     * @brief Construct a new Message object with a specific label and amount of message data
     *
     * @param label - message's label
     * @param size - message's size
     * @param data - message's data
     */         
    Message(Label label, uint16_t size, std::byte *data) : m_data(data), m_label(label), m_size(size) {
    }   
        
    /**
     * @brief Return this message instance's data as a pointer to an object of type T
     *
     * @tparam T - a POD type
     * @return T - result of the conversion, or nullptr for a size mismatch or invalid type
     */
    template <typename T>
    T *as() {
        if (m_data != nullptr && sizeof(T) == m_size && std::is_trivially_copyable<T>()) {
            return reinterpret_cast<T *>(m_data);
        }
        return nullptr;
    }

    /**
     * @brief Data associated with this Message. This will be nullptr in the case of signals
     */
    std::byte *m_data = nullptr;

    /**
     * @brief Label associated with the Mailbox message or signal
     */
    Label m_label = 0;

    /**
     * @brief Size of the Mailbox message data
     */
    uint16_t m_size = 0;
};

} // namespace msglib