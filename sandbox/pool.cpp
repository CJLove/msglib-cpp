#include "msglib/detail/BytePool.h"
#include "DiagResource.h"
#include <spdlog/spdlog.h>

#include <array>
#include <iostream>
#include <memory_resource>
#include <unistd.h>

// Test program to exercise the BytePool class with instrumented PMR resources to
// track allocation/deallocations

int main(int argc, char** argv) {
    size_t storageSize = 16384; // NOLINT
    size_t capacity = 16;       // NOLINT
    size_t elementSize = 16;    // NOLINT
    spdlog::level::level_enum log = spdlog::level::trace;
    int c;

    while ((c = getopt(argc,argv,"s:c:e:l:?")) != EOF) { // NOLINT
        switch (c) {
        case 's':
            storageSize = std::stoul(optarg,nullptr,10); // NOLINT
            break;
        case 'c':
            capacity = std::stoul(optarg,nullptr,10);    // NOLINT
            break;
        case 'e':
            elementSize = std::stoul(optarg,nullptr,10);  // NOLINT
            break;
        case 'l':
            log = static_cast<spdlog::level::level_enum>(std::stoul(optarg,nullptr,10)); // NOLINT
            break;
        case '?':
            spdlog::error("Usage:\npool -s <storageSize> -c <capacity> -e <elementSize> -l <logLevel>");
            exit(1); // NOLINT
        }
    }

    spdlog::set_level(log);

    // Catch/log any rogue PMR allocations
    msglib::detail::DiagResource default_alloc {"Rogue PMR Allocation!", std::pmr::null_memory_resource()};
    std::pmr::set_default_resource(&default_alloc);

    // Catch/log any OOM conditions
    msglib::detail::DiagResource oom {"Out of Memory", std::pmr::null_memory_resource()};

    // Underlying storage
    std::unique_ptr<std::byte[]> storage = std::make_unique<std::byte[]>(storageSize);
    spdlog::info("storage: {} - {}", fmt::ptr(storage.get()), fmt::ptr(storage.get() + storageSize));

    // Monotonic buffer resource using storage bytes
    std::pmr::monotonic_buffer_resource bufferResource(storage.get(), storageSize, &oom);

    // Catch/log any monotonic buffer resource allocations
    msglib::detail::DiagResource monotonicResource("Monotonic", &bufferResource);

    // Pool resource using buffer resource
    std::pmr::synchronized_pool_resource poolResource(&monotonicResource);

    spdlog::info("msglib::detail::BytePool({},{})", capacity, elementSize);
    msglib::detail::BytePool pool(elementSize, capacity, &poolResource);

    std::vector<msglib::detail::DataBlock> elements;
    elements.resize(capacity);

    for (size_t i = 0; i < capacity; i++) {
        spdlog::info("Pool::size() = {}", pool.size());

        elements[i] = pool.alloc();
    }

    spdlog::info("Pool::size() = {}", pool.size());

    for (size_t i = 0; i < capacity; i++) {
        spdlog::info("Element {} size {} addr {}", i, elements[i].size(), fmt::ptr(elements[i].get()));
    }

    auto blk = pool.alloc();

    if (blk.get() != nullptr) {
        spdlog::error("Non-nullptr value returned by BytePool::alloc() when pool is empty");
    }

    spdlog::info("Freeing elements");

    for (size_t i = 0; i < capacity; i++) {
        pool.free(elements[i].get());
        spdlog::info("Pool::size() = {}", pool.size());
    }

    return 0;
}