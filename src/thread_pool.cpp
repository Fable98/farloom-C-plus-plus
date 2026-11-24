#include "thread_pool.hpp"

namespace farloom {

std::unique_ptr<ThreadPool<Job>> make_closure_pool(size_t size, size_t capacity) {
    return std::make_unique<ThreadPool<Job>>(size, capacity, [](Job job) { job(); });
}

} // namespace farloom
