#include "test_framework.hpp"
#include "thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <set>
#include <string>

using namespace farloom;

void test_pool_named_workers() {
    auto pool = make_closure_pool(3, 3);
    ASSERT_EQ(pool->size(), 3);

    std::mutex m;
    std::condition_variable cv;
    std::set<std::string> worker_names;
    std::atomic<int> started{0};
    std::atomic<bool> release{false};

    for (int i = 0; i < 3; ++i) {
        auto res = pool->try_submit([&]() {
            std::string name = logger::current_worker();
            {
                std::lock_guard<std::mutex> lock(m);
                worker_names.insert(name);
            }
            started++;
            while (!release.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        ASSERT_TRUE(res.is_ok());
    }

    while (started.load() < 3) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    {
        std::lock_guard<std::mutex> lock(m);
        ASSERT_EQ(worker_names.size(), 3);
        ASSERT_TRUE(worker_names.contains("worker-0"));
        ASSERT_TRUE(worker_names.contains("worker-1"));
        ASSERT_TRUE(worker_names.contains("worker-2"));
    }

    release.store(true);
    pool->shutdown();
}

void test_full_queue_rejection() {
    std::atomic<bool> release{false};
    std::atomic<int> started{0};

    auto pool = std::make_unique<ThreadPool<int>>(1, 1, [&](int val) {
        started++;
        if (val == 1) {
            while (!release.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    });

    ASSERT_TRUE(pool->try_submit(1).is_ok());
    while (started.load() < 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Worker is occupied with job 1. Job 2 fills the single queue slot.
    ASSERT_TRUE(pool->try_submit(2).is_ok());

    // Job 3 must fail with QueueSubmitError::Full
    auto r3 = pool->try_submit(3);
    ASSERT_TRUE(r3.is_err());
    ASSERT_EQ(r3.error().first, QueueSubmitError::Full);
    ASSERT_EQ(r3.error().second, 3);

    release.store(true);
    pool->shutdown();
}

void test_drain_on_shutdown() {
    std::atomic<int> counter{0};
    std::atomic<bool> release{false};
    std::atomic<bool> gate_started{false};

    auto pool = make_closure_pool(1, 10);

    ASSERT_TRUE(pool->try_submit([&]() {
        gate_started.store(true);
        while (!release.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }).is_ok());

    while (!gate_started.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    for (int i = 0; i < 6; ++i) {
        ASSERT_TRUE(pool->try_submit([&]() {
            counter++;
        }).is_ok());
    }

    ASSERT_EQ(counter.load(), 0);

    release.store(true);
    pool->shutdown();
    ASSERT_EQ(counter.load(), 6);
}

void test_panicking_job_does_not_kill_worker() {
    auto pool = make_closure_pool(1, 4);

    ASSERT_TRUE(pool->try_submit([]() {
        throw std::runtime_error("intentional test panic");
    }).is_ok());

    std::atomic<int> after{0};
    for (int i = 0; i < 2; ++i) {
        ASSERT_TRUE(pool->try_submit([&]() {
            after++;
        }).is_ok());
    }

    pool->shutdown();
    ASSERT_EQ(after.load(), 2);
}

int main() {
    RUN_TEST(test_pool_named_workers);
    RUN_TEST(test_full_queue_rejection);
    RUN_TEST(test_drain_on_shutdown);
    RUN_TEST(test_panicking_job_does_not_kill_worker);
    return 0;
}
