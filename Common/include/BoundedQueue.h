// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>

/**
 * @brief A bounded, thread-safe FIFO queue.
 *
 * The queue stores elements in a fixed-size ring buffer and synchronizes
 * access with a mutex and condition variables.
 *
 * push() blocks while the queue is full. pop() blocks while the queue is
 * empty. Calling stop() wakes blocked producers and consumers, prevents
 * further insertion, and allows consumers to drain any remaining items.
 *
 * @tparam T Type of object stored in the queue.
 * @tparam Capacity Maximum number of elements the queue can hold.
 */
template <typename T, std::size_t Capacity>
class BoundedQueue
{
    static_assert(Capacity > 0, "BoundedQueue capacity must be greater than zero");

public:
    /**
     * @brief Constructs an empty queue.
     */
    BoundedQueue() = default;

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    /**
     * @brief Adds an item to the queue, blocking while the queue is full.
     *
     * @param item Item to add.
     * @return true if the item was added.
     * @return false if the queue was stopped.
     */
    bool push(T item)
    {
        std::unique_lock<std::mutex> lock(mMutex);

        mNotFull.wait(lock, [this]()
                      {
                          return mStopped || mSize < Capacity;
                      });

        if (mStopped)
        {
            return false;
        }

        mStorage[mWriteIndex].emplace(std::move(item));
        mWriteIndex = nextIndex(mWriteIndex);
        ++mSize;

        lock.unlock();
        mNotEmpty.notify_one();

        return true;
    }

    /**
     * @brief Attempts to add an item without blocking.
     *
     * @param item Item to add.
     * @return true if the item was added.
     * @return false if the queue is full or stopped.
     */
    bool tryPush(T item)
    {
        std::unique_lock<std::mutex> lock(mMutex);

        if (mStopped || mSize == Capacity)
        {
            return false;
        }

        mStorage[mWriteIndex].emplace(std::move(item));
        mWriteIndex = nextIndex(mWriteIndex);
        ++mSize;

        lock.unlock();
        mNotEmpty.notify_one();

        return true;
    }

    /**
     * @brief Removes and returns the next item, blocking while the queue is empty.
     *
     * Consumers may continue draining queued items after stop() is called.
     *
     * @return The next item.
     * @return std::nullopt if the queue is stopped and empty.
     */
    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lock(mMutex);

        mNotEmpty.wait(lock, [this]()
                       {
                           return mStopped || mSize > 0;
                       });

        if (mSize == 0)
        {
            return std::nullopt;
        }

        T item = std::move(*mStorage[mReadIndex]);
        mStorage[mReadIndex].reset();

        mReadIndex = nextIndex(mReadIndex);
        --mSize;

        lock.unlock();
        mNotFull.notify_one();

        return item;
    }

    /**
     * @brief Attempts to remove and return the next item without blocking.
     *
     * @return The next item if one is available.
     * @return std::nullopt if the queue is empty.
     */
    std::optional<T> tryPop()
    {
        std::unique_lock<std::mutex> lock(mMutex);

        if (mSize == 0)
        {
            return std::nullopt;
        }

        T item = std::move(*mStorage[mReadIndex]);
        mStorage[mReadIndex].reset();

        mReadIndex = nextIndex(mReadIndex);
        --mSize;

        lock.unlock();
        mNotFull.notify_one();

        return item;
    }

    /**
     * @brief Stops the queue and wakes all blocked threads.
     *
     * After stop() is called, push() and tryPush() reject new items.
     * Consumers may continue removing items already stored in the queue.
     */
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStopped = true;
        }

        mNotEmpty.notify_all();
        mNotFull.notify_all();
    }

    /**
     * @brief Returns the current number of queued items.
     */
    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mSize;
    }

    /**
     * @brief Returns the maximum number of items the queue can hold.
     */
    [[nodiscard]] static constexpr std::size_t capacity() noexcept
    {
        return Capacity;
    }

    /**
     * @brief Reports whether the queue currently contains no items.
     */
    [[nodiscard]] bool empty() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mSize == 0;
    }

    /**
     * @brief Reports whether stop() has been called.
     */
    [[nodiscard]] bool stopped() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mStopped;
    }

private:
    /**
     * @brief Advances a ring-buffer index, wrapping at Capacity.
     *
     * @param index Current index.
     * @return The next index.
     */
    [[nodiscard]] static constexpr std::size_t nextIndex(
        std::size_t index) noexcept
    {
        return (index + 1) % Capacity;
    }

    std::array<std::optional<T>, Capacity> mStorage{};

    std::size_t mReadIndex = 0;
    std::size_t mWriteIndex = 0;
    std::size_t mSize = 0;

    mutable std::mutex mMutex;
    std::condition_variable mNotEmpty;
    std::condition_variable mNotFull;

    bool mStopped = false;
};
