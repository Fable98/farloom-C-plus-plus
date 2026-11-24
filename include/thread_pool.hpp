#pragma once

#include "bounded_queue.hpp"
#include "logger.hpp"
#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <string>
#include <format>
#include <pthread.h>
#include <stdexcept>

namespace farloom {

template <typename J>
class ThreadPool {
public:
    using Handler = std::function<void(J)>;

    ThreadPool(size_t size, size_t capacity, Handler handler)
        : queue_(std::make_shared<BoundedQueue<J>>(capacity)),
          handler_(std::make_shared<Handler>(std::move(handler))),
          running_(true) {
        if (size == 0) throw std::invalid_argument("thread pool size must be at least 1");
        if (capacity == 0) throw std::invalid_argument("thread pool queue capacity must be at least 1");

        workers_.reserve(size);
        for (size_t id = 0; id < size; ++id) {
            workers_.emplace_back([id, q = queue_, h = handler_]() {
                std::string worker_name = std::format("worker-{}", id);
                logger::set_thread_label(worker_name);
#if defined(__APPLE__)
                pthread_setname_np(worker_name.c_str());
#elif defined(__linux__)
                pthread_setname_np(pthread_self(), worker_name.c_str());
#endif
                while (true) {
                    auto item = q->pop();
                    if (!item.has_value()) {
                        logger::info_event("worker_exit", std::format("worker={}", worker_name));
                        break;
                    }
                    try {
                        (*h)(std::move(*item));
                    } catch (const std::exception& e) {
                        logger::error("job_panic", e.what());
                    } catch (...) {
                        logger::error("job_panic", "<unknown exception>");
                    }
                }
                logger::set_thread_label(std::nullopt);
            });
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    Result<void, std::pair<QueueSubmitError, J>> try_submit(J job) {
        if (!running_) {
            return Result<void, std::pair<QueueSubmitError, J>>::err(
                {QueueSubmitError::Closed, std::move(job)}
            );
        }
        return queue_->try_push(std::move(job));
    }

    void shutdown() {
        if (running_) {
            running_ = false;
            if (queue_) {
                queue_->close();
            }
            for (auto& t : workers_) {
                if (t.joinable()) {
                    t.join();
                }
            }
        }
    }

    size_t size() const noexcept {
        return workers_.size();
    }

private:
    std::shared_ptr<BoundedQueue<J>> queue_;
    std::shared_ptr<Handler> handler_;
    std::vector<std::thread> workers_;
    bool running_{false};
};

using Job = std::function<void()>;

std::unique_ptr<ThreadPool<Job>> make_closure_pool(size_t size, size_t capacity);

} // namespace farloom
