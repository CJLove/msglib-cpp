#include "msglib/detail/Queue.h"
#include "msglib/Message.h"
#include "msglib/detail/DiagResource.h"
#include <spdlog/spdlog.h>

#include <array>
#include <atomic>
#include <chrono>
#include <memory_resource>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

// Test program to exercise the Queue class with instrumented PMR resources to
// track allocation/deallocations

std::atomic_bool running = true;

void consumer(msglib::detail::Queue<msglib::Message> &queue) {
    msglib::Message msg;
    while (running.load()) {
        if (queue.popWait(msg, 500ms)) {
            spdlog::info("Received msg({} {})", msg.m_label, msg.m_size);
        }
    }
}

void producer(msglib::detail::Queue<msglib::Message> &queue, size_t count) {
    uint32_t i = 0;
    std::this_thread::sleep_for(1s);

    while (i < count) {
        if (!queue.emplace(i, i + 1, nullptr)) {
            spdlog::error("emplace() failed");
        }
        i++;
        std::this_thread::sleep_for(100ms);
    }
}

int main(int argc, char **argv) {
    size_t storageSize = 12228;
    size_t count = 10;
    size_t queueSize = 5;
    spdlog::level::level_enum log = spdlog::level::trace;

    int c;
    while ((c = getopt(argc, argv, "s:c:q:l:?")) != EOF) {
        switch (c) {
        case 's':
            storageSize = std::stoul(optarg, nullptr, 10);
            break;
        case 'c':
            count = std::stoul(optarg, nullptr, 10);
            break;
        case 'q':
            queueSize = std::stoul(optarg, nullptr, 10);
            break;
        case 'l':
            log = static_cast<spdlog::level::level_enum>(std::stoi(optarg, nullptr, 10));
            break;
        case '?':
            spdlog::error("Usage\nqueue -s <storage> -c <count> -q <queueSize> -l <loglevel>");
            exit(1);
        }
    }

    spdlog::set_level(log);
    spdlog::info("Storage size {} queue size {}", storageSize, queueSize);
    spdlog::info("sizeof(msglib::Message) = {}", sizeof(msglib::Message));

    // Catch/log any rogue PMR allocations
    msglib::detail::DiagResource default_alloc {"Rogue PMR Allocation!", std::pmr::null_memory_resource()};
    std::pmr::set_default_resource(&default_alloc);

    // Catch/log any OOM conditions
    msglib::detail::DiagResource oom {"Out of Memory", std::pmr::null_memory_resource()};

    // Underlying storage
    std::unique_ptr<std::byte[]> storage = std::make_unique<std::byte[]>(storageSize);
    spdlog::info("storage: {} - {}", fmt::ptr(storage), fmt::ptr(storage.get() + storageSize));

    // Monotonic buffer resource using storage bytes
    std::pmr::monotonic_buffer_resource bufferResource(storage.get(), storageSize, &oom);

    // Catch/log any monotonic buffer resource allocations
    msglib::detail::DiagResource monotonicResource("Monotonic", &bufferResource);

    // Pool resource using buffer resource
    std::pmr::synchronized_pool_resource poolResource(&monotonicResource);

    // Queue using the pool resource
    spdlog::info("msglib::detail::Queue<msglib::Message>({})",queueSize);
    msglib::detail::Queue<msglib::Message> queue(queueSize, &poolResource);

    spdlog::info("sizeof(msglib::detail::Queue<Message>) = {}", sizeof(queue));

    for (size_t i = 0; i < queueSize + 1; i++) {
        if (!queue.emplace(i, 16, nullptr)) {
            spdlog::error("emplace() failed");
        }
    }

    std::thread t1(consumer, std::ref(queue));
    producer(queue, count);

    running = false;
    t1.join();

    spdlog::info("Queue going out of scope");
}