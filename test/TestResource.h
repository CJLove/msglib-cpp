#pragma once

#include <memory_resource>

class TestResource : public std::pmr::memory_resource {
public:
    TestResource() = default;

    [[nodiscard]] bool allocatorInvoked() const {
        return m_invoked;
    }

private:
    bool m_invoked = false;

    void *do_allocate(std::size_t, std::size_t) override { // NOLINT
        m_invoked = true;
        return nullptr;
    }

    void do_deallocate(void *, std::size_t, std::size_t) override { // NOLINT
        m_invoked = true;
    }

    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
        return this == &other;
    }
};