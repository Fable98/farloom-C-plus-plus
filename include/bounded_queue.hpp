#pragma once

#include "result.hpp"
#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>
#include <utility>
#include <cstddef>

namespace farloom {

enum class QueueSubmitError {
    Full,
    Closed,
};

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity) : capacity_(capacity), closed_(false) {}

    Result<void, std::pair<QueueSubmitError, T>> try_push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (closed_) {
            return Result<void, std::pair<QueueSubmitError, T>>::err(
                {QueueSubmitError::Closed, std::move(item)}
            );
        }
        if (queue_.size() >= capacity_) {
            return Result<void, std::pair<QueueSubmitError, T>>::err(
                {QueueSubmitError::Full, std::move(item)}
            );
        }
        queue_.push(std::move(item));
        cv_.notify_one();
        return Result<void, std::pair<QueueSubmitError, T>>::ok();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() {
            return !queue_.empty() || closed_;
        });
        if (queue_.empty() && closed_) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    bool is_closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    size_t capacity() const noexcept {
        return capacity_;
    }

private:
    const size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    bool closed_;
};

} // namespace farloom
