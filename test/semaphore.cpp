//
// Created by federico on 01/07/2020.
//

#include <atomic>
#include <thread>
#include <functional>
#include "gtest/gtest.h"
#include "../Semaphore.h"

class SemaphoreTest : public ::testing::Test {

protected:

    void SetUp() override {
        counter = 0;
        semaphore = new CountingSemaphore;
        threads.reserve(kThreads);
        counter_mutex = new std::mutex;
    }

    void TearDown() override {
        delete semaphore;
        delete counter_mutex;
    }

    //generates the function run by the threads
    bool test_using(std::function<void(CountingSemaphore*, std::atomic<int>&, std::mutex*, bool, bool)> test_function, bool more_acquires, bool try_acquire) {

        for (int t=0; t<kThreads; t++) {
            threads.push_back(new std::thread([&, more_acquires, try_acquire](){
                std::this_thread::sleep_for(std::chrono::milliseconds (t));
                for (int i=0; i<100; i++) {
                    test_function(semaphore, counter, counter_mutex, more_acquires, try_acquire);
                    std::this_thread::sleep_for(std::chrono::milliseconds (10));
                }
            }));
        }

        bool correct = true;
        std::thread checker([&] () {
            for (int i = 0; i < 1000; i++) {
                std::unique_lock<std::mutex> lk(*counter_mutex);
                correct &= counter >= 0;
                std::this_thread::sleep_for(std::chrono::milliseconds (1));
            }
        });

        checker.join();

        return correct;

    }

    std::atomic<int> counter = 0;
    const int kThreads = 5;
    std::vector<std::thread*> threads;
    CountingSemaphore* semaphore = nullptr;
    std::mutex* counter_mutex;

};

/// Function used by the threads to test the semaphore.
///
/// \param s Semaphore being tested.
/// \param counter Atomic variable used to keep track of the operations.
/// \param more_acquires If true, function does more acquires then releases (80% - 20%).
/// If false, it does approximately the same number (50% - 50%).
/// \param try_acquire If true uses try_acquire (non-blocking). If false uses acquire (blocking).
void random_release_or_acquire(CountingSemaphore* s, std::atomic<int>& counter, std::mutex* m, bool more_acquires, bool try_acquire) {
    std::unique_lock<std::mutex> lk(*m);
    if (rand()%100 < (more_acquires ? 80 : 50)) {
        if (try_acquire and s->try_acquire()) {
            counter--;
        }
        if (!try_acquire) {
            s->acquire();
            counter--;
        }
    } else {
        s->release();
        counter++;
    }
}

// Test with more_acquires: true and try_acquire: true (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_MoreAcquires_TryAcquire) {
    bool correct = test_using(random_release_or_acquire, true, true);
    ASSERT_TRUE(correct);
}

// Test with more_acquires: false and try_acquire: true (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_noMoreAcquires_TryAcquire) {
    bool correct = test_using(random_release_or_acquire, false, false);
    ASSERT_TRUE(correct);
}

// Test with more_acquires: true and try_acquire: false (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_MoreAcquires_noTryAcquire) {
    bool correct = test_using(random_release_or_acquire, true, false);
    ASSERT_TRUE(correct);
}


// Test with more_acquires: false and try_acquire: false (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_noMoreAcquires_noTryAcquire) {
    bool correct = test_using(random_release_or_acquire, false, false);
    ASSERT_TRUE(correct);
}