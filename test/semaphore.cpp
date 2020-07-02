//
// Created by federico on 01/07/2020.
//

#include <atomic>
#include <thread>
#include <functional>
#include <random>
#include "gtest/gtest.h"
#include "../Semaphore.h"

class SemaphoreTest : public ::testing::Test {

protected:

    /// This method can be called by create some threads calling a function repeatedly
    ///
    /// \param test_function The function that will be called by the threads
    /// \param more_acquires Argument passed to the function
    /// \param try_acquire Argument passed to the function
    /// \param operations Number of times the function is called
    /// \return True if the counter is correct (>= 0), False otherwise
    bool test_using(
        std::function<void(CountingSemaphore&, std::atomic<int>&, bool, bool)> test_function,
        bool more_acquires,
        bool try_acquire,
        int operations) {

        threads.reserve(kThreads);

        for (int t=0; t<kThreads; t++) {
            threads.emplace_back([&, more_acquires, try_acquire](){
                for (int i=0; i<operations; i++)
                    test_function(semaphore, counter, more_acquires, try_acquire);
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        for (auto& t:threads)
            t.detach();

        std::cerr<<counter<<std::endl;
        return counter >= 0;
    }

    std::atomic<int> counter = 0;
    const int kThreads = 5;
    CountingSemaphore semaphore;
    std::vector<std::thread> threads;

};

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dist(0, 99);

/// Function used by the threads to test the semaphore.
///
/// \param s Semaphore being tested.
/// \param counter Atomic variable used to keep track of the operations.
/// \param more_acquires If true, function does more acquires then releases (80% - 20%).
/// If false, it does approximately the same number (50% - 50%).
/// \param try_acquire If true uses try_acquire (non-blocking). If false uses acquire (blocking).
void random_release_or_acquire(CountingSemaphore& s, std::atomic<int>& counter, bool more_acquires, bool try_acquire) {
    if (dist(gen)%100 < (more_acquires ? 80 : 40)) {
        if (try_acquire and s.try_acquire()) {
            counter--;
        }
        if (!try_acquire) {
            s.acquire();
            counter--;
        }
    } else {
        s.release();
        counter++;
    }
}


// Tests with more_acquires: true and try_acquire: true (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_MoreAcquires_TryAcquire_FewOperations) {
    ASSERT_TRUE(test_using(random_release_or_acquire, true, true, 100));
}

TEST_F(SemaphoreTest, semaphore_MoreAcquires_TryAcquire_ManyOperations) {
    ASSERT_TRUE(test_using(random_release_or_acquire, true, true, 100000));
}


// Tests with more_acquires: false and try_acquire: true (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_noMoreAcquires_TryAcquire_FewOperations) {
    ASSERT_TRUE(test_using(random_release_or_acquire, false, true, 100));
}

TEST_F(SemaphoreTest, semaphore_noMoreAcquires_TryAcquire_ManyOperations) {
    ASSERT_TRUE(test_using(random_release_or_acquire, false, true, 100000));
}


// Tests with more_acquires: false and try_acquire: false (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_noMoreAcquires_noTryAcquire_FewOperations) {
    ASSERT_TRUE(test_using(random_release_or_acquire, false, false, 100));
}

TEST_F(SemaphoreTest, semaphore_noMoreAcquires_noTryAcquire_ManyOperations) {
    ASSERT_TRUE(test_using(random_release_or_acquire, false, false, 100000));
}