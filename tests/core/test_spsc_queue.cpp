#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <thread>

#include "core/SpscQueue.h"

namespace {

// Capacity 64 -> 63 usable slots.
constexpr std::size_t kCapacity = 64;
using Queue = drumas::core::SpscQueue<int, kCapacity>;

}  // namespace

TEST_CASE("spsc queue is empty when constructed", "[core][spsc]") {
    Queue queue;
    int value = -1;

    REQUIRE(queue.empty());
    REQUIRE(queue.sizeApprox() == 0);
    REQUIRE_FALSE(queue.pop(value));
    REQUIRE(Queue::capacity() == kCapacity - 1);
}

TEST_CASE("spsc queue preserves FIFO order", "[core][spsc]") {
    Queue queue;
    for (int i = 0; i < 5; ++i) {
        REQUIRE(queue.push(i));
    }
    REQUIRE(queue.sizeApprox() == 5);

    for (int i = 0; i < 5; ++i) {
        int value = -1;
        REQUIRE(queue.pop(value));
        REQUIRE(value == i);
    }
    REQUIRE(queue.empty());
}

TEST_CASE("spsc queue rejects pushes beyond its usable capacity", "[core][spsc]") {
    Queue queue;
    for (std::size_t i = 0; i < Queue::capacity(); ++i) {
        REQUIRE(queue.push(static_cast<int>(i)));
    }
    REQUIRE(queue.sizeApprox() == Queue::capacity());

    REQUIRE_FALSE(queue.push(999));

    // One pop makes room for exactly one more element.
    int value = -1;
    REQUIRE(queue.pop(value));
    REQUIRE(queue.push(1000));
    REQUIRE(queue.sizeApprox() == Queue::capacity());
}

TEST_CASE("spsc queue drains back to empty", "[core][spsc]") {
    Queue queue;
    for (std::size_t i = 0; i < Queue::capacity(); ++i) {
        REQUIRE(queue.push(static_cast<int>(i)));
    }

    std::size_t took = 0;
    int value = -1;
    while (queue.pop(value)) {
        REQUIRE(value == static_cast<int>(took));
        ++took;
    }
    REQUIRE(took == Queue::capacity());
    REQUIRE(queue.empty());
}

TEST_CASE("spsc queue hands items across two threads in order", "[core][spsc]") {
    constexpr int kItems = 200000;

    Queue queue;
    long long producedSum = 0;

    std::thread producer([&queue, &producedSum] {
        for (int i = 0; i < kItems; ++i) {
            producedSum += i;
            while (!queue.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    long long consumedSum = 0;
    int expected = 0;
    bool inOrder = true;
    int value = -1;
    while (expected < kItems) {
        if (queue.pop(value)) {
            inOrder = inOrder && (value == expected);
            consumedSum += value;
            ++expected;
        } else {
            std::this_thread::yield();
        }
    }

    producer.join();

    REQUIRE(inOrder);
    REQUIRE(consumedSum == producedSum);
    REQUIRE(queue.empty());
}
