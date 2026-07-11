// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

template <typename T>
class BoundedQueue
{
public:
    explicit BoundedQueue(std::size_t capacity)
        : mCapacity(capacity)
    {
        assert(mCapacity > 0);
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    bool push(T item)
    {
        std::unique_lock<std::mutex> lock(mMutex);

        mNotFull.wait(lock, [this]()
        {
            return mStopped || mQueue.size() < mCapacity;
        });

        if (mStopped)
        {
            return false;
        }

        mQueue.push(std::move(item));

        lock.unlock();
        mNotEmpty.notify_one();

        return true;
    }

    bool tryPush(T item)
    {
        std::unique_lock<std::mutex> lock(mMutex);

        if (mStopped || mQueue.size() >= mCapacity)
        {
            return false;
        }

        mQueue.push(std::move(item));

        lock.unlock();
        mNotEmpty.notify_one();

        return true;
    }

    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lock(mMutex);

        mNotEmpty.wait(lock, [this]()
        {
            return mStopped || !mQueue.empty();
        });

        if (mQueue.empty())
        {
            return std::nullopt;
        }

        T item = std::move(mQueue.front());
        mQueue.pop();

        lock.unlock();
        mNotFull.notify_one();

        return item;
    }

    std::optional<T> tryPop()
    {
        std::unique_lock<std::mutex> lock(mMutex);

        if (mQueue.empty())
        {
            return std::nullopt;
        }

        T item = std::move(mQueue.front());
        mQueue.pop();

        lock.unlock();
        mNotFull.notify_one();

        return item;
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStopped = true;
        }

        mNotEmpty.notify_all();
        mNotFull.notify_all();
    }

    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mQueue.size();
    }

    [[nodiscard]] std::size_t capacity() const noexcept
    {
        return mCapacity;
    }

    [[nodiscard]] bool empty() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mQueue.empty();
    }

    [[nodiscard]] bool stopped() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mStopped;
    }

private:
    const std::size_t mCapacity;

    mutable std::mutex mMutex;
    std::condition_variable mNotEmpty;
    std::condition_variable mNotFull;
    std::queue<T> mQueue;

    bool mStopped = false;
};
