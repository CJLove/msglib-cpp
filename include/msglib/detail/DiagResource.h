#pragma once

#include <spdlog/spdlog.h>

#include <memory_resource>

namespace msglib::detail {

class TestResource : public std::pmr::memory_resource {
public:
    TestResource(): m_invoked(false) {
    }

    bool allocatorInvoked() const {
        return m_invoked;
    }

private:
    bool m_invoked;

    void *do_allocate(std::size_t, std::size_t) override {
        m_invoked = true;
        return nullptr;
    }

    void do_deallocate(void *, std::size_t, std::size_t) override {
        m_invoked = true;
    }

    bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
        return this == &other;
    }
};

/**
 * @brief DiagResource realizes the std::pmr::memory_resource interface and logs calls to 
 *        do_allocate() and do_deallocate() calls
 */
class DiagResource : public std::pmr::memory_resource {
public:
    /**
     * @brief Construct a new DiagResource object
     * 
     * @param name - name of this specific instance
     * @param upstream - upstream memory_resource to allocate/deallocate from
     */
    DiagResource(std::string name, std::pmr::memory_resource *upstream) : m_name(std::move(name)), m_upstream(upstream) {
        assert(upstream);
    }

private:
    /**
     * @brief Name of this DiagResource instance
     */
    std::string m_name;

    /**
     * @brief Upstream memory_resources
     */
    std::pmr::memory_resource *m_upstream;

    /**
     * @brief Log a request to allocate bytes from the memory_resource
     * 
     * @param bytes - number of bytes to allocate
     * @param alignment - alignment requirements
     * @return void* - pointer to the allocated bytes
     */
    void *do_allocate(std::size_t bytes, std::size_t alignment) override {
        spdlog::trace("[{} (alloc)] Size: {} Alignment: {} ...", m_name, bytes, alignment);
        auto result = m_upstream->allocate(bytes, alignment);
        spdlog::trace("[{} (alloc)] ... Address: {}", m_name, result);
        return result;
    }

    /**
     * @brief Log a request to free bytes and return them to the memory_resources
     * 
     * @param p - bytes to be freed
     * @param bytes - number of bytes to free
     * @param alignment - alignment requirements 
     */
    void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) override {
        spdlog::trace("[{} (dealloc)] Address: {} Dealloc Size: {} Alignment: {}", m_name, p, bytes, alignment);
        m_upstream->deallocate(p, bytes, alignment);
    }

    /**
     * @brief Return whether this resource is equal to another memory_resources
     * 
     * @param other - memory_resource for comparison
     * @return true - resources are equal
     * @return false - resources aren't equal
     */
    bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
        return this == &other;
    }
};

} // namespace msglib::detail