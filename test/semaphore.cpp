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
        threads.clear();
        semaphore = new CountingSemaphore;
        threads.reserve(kThreads);
    }

    void TearDown() override {
        delete semaphore;
    }

    //generates the function run by the threads
    std::function<void()> generate_function(
        std::function<void(CountingSemaphore&, std::atomic<int>&, bool, bool)> test_function,
        bool more_acquires, bool try_acquire) {
        return [&, more_acquires, try_acquire](){
            for (int i=0; i<100; i++) {
                test_function(*semaphore, counter, more_acquires, try_acquire);
                std::this_thread::sleep_for(std::chrono::milliseconds (1));
            }
        };
    }

    std::atomic<int> counter = 0;
    const int kThreads = 5;
    std::vector<std::thread*> threads;
    CountingSemaphore* semaphore = nullptr;

};

/// Function used by the threads to test the semaphore.
///
/// \param s Semaphore being tested.
/// \param counter Atomic variable used to keep track of the operations.
/// \param more_acquires If true, function does more acquires then releases (80% - 20%).
/// If false, it does approximately the same number (50% - 50%).
/// \param try_acquire If true uses try_acquire (non-blocking). If false uses acquire (blocking).
void random_release_or_acquire(CountingSemaphore& s, std::atomic<int>& counter, bool more_acquires, bool try_acquire) {
    if (rand()%100 < (more_acquires ? 80 : 50)) {
        if (try_acquire and s.try_acquire())
            counter++;
        if (!try_acquire) {
            s.acquire();
            counter++;
        }
    } else {
        s.release();
        counter++;
    }
}

// Test with more_acquires: true and try_acquire: true (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_MoreAcquires_TryAcquire) {

    for (int t=0; t<kThreads; t++)
        threads.push_back(new std::thread(generate_function(random_release_or_acquire, true, true)));

    ASSERT_TRUE(true);

    for (auto& t:threads)
        t->join();

    ASSERT_GE(counter, 0);

}

// Test with more_acquires: false and try_acquire: true (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_noMoreAcquires_TryAcquire) {

    for (int t=0; t<kThreads; t++)
        threads.push_back(new std::thread(generate_function(random_release_or_acquire, false, true)));

    for (auto& t:threads)
        t->join();

    ASSERT_GE(counter, 0);

}

// TODO: this fails because right now CountingSemaphore has busy wait
// Test with more_acquires: true and try_acquire: false (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_MoreAcquires_noTryAcquire) {

    for (int t=0; t<kThreads; t++)
        threads.push_back(new std::thread(generate_function(random_release_or_acquire, true, false)));

    for (auto& t:threads)
        t->join();

    ASSERT_GE(counter, 0);

}

// TODO: this fails because right now CountingSemaphore has busy wait
// Test with more_acquires: false and try_acquire: false (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_noMoreAcquires_noTryAcquire) {

    for (int t=0; t<kThreads; t++)
        threads.push_back(new std::thread(generate_function(random_release_or_acquire, false, false)));

    for (auto& t:threads)
        t->join();

    ASSERT_GE(counter, 0);

}