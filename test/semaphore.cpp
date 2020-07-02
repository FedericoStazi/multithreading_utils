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

    ~SemaphoreTest() override {
        semaphore.clear();
        for (auto& t:threads)
            t.join();
    }

    SemaphoreTest() {
        srand(std::chrono::system_clock::now().time_since_epoch().count());
    }

    /// This method can be called by create some threads calling a function repeatedly
    ///
    /// \param test_function The function that will be called by the threads
    /// \param more_acquires Argument passed to the function
    /// \param try_acquire Argument passed to the function
    /// \param operations Number of times the function is called
    /// \return True if the counter is correct (>= 0), False otherwise
    bool test_using(bool more_acquires, bool try_acquire, int operations) {

        threads.reserve(kThreads);

        for (int t=0; t<kThreads; t++) {
            threads.emplace_back([=](){
                for (int i=0; i<operations; i++) {
                    if (rand()%100 < (more_acquires ? 80 : 40)) {
                        if (try_acquire and semaphore.try_acquire()) {
                            counter--;
                        }
                        if (!try_acquire) {
                            semaphore.acquire();
                            counter--;
                        }
                    } else {
                        semaphore.release();
                        counter++;
                    }
                }
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cerr<<"counter: "<<counter<<std::endl;
        return counter >= 0;
    }

    std::atomic<int> counter = 0;
    const int kThreads = 5;
    CountingSemaphore semaphore;
    std::vector<std::thread> threads;

};

// Tests with more_acquires: true and try_acquire: true (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_MoreAcquires_TryAcquire_FewOperations) {
    ASSERT_TRUE(test_using(true, true, 100));
}

TEST_F(SemaphoreTest, semaphore_MoreAcquires_TryAcquire_ManyOperations) {
    ASSERT_TRUE(test_using(true, true, 10000));
}


// Tests with more_acquires: false and try_acquire: true (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_LessAcquires_TryAcquire_FewOperations) {
    ASSERT_TRUE(test_using(false, true, 100));
}

TEST_F(SemaphoreTest, semaphore_LessAcquires_TryAcquire_ManyOperations) {
    ASSERT_TRUE(test_using(false, true, 10000));
}


// Tests with more_acquires: true and try_acquire: false (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_MoreAcquires_noTryAcquire_FewOperations) {
    ASSERT_TRUE(test_using(true, false, 100));
}

TEST_F(SemaphoreTest, semaphore_MoreAcquires_noTryAcquire_ManyOperations) {
    ASSERT_TRUE(test_using(true, false, 10000));
}


// Tests with more_acquires: false and try_acquire: false (see random_release_or_acquire documentation)
TEST_F(SemaphoreTest, semaphore_LessAcquires_noTryAcquire_FewOperations) {
    ASSERT_TRUE(test_using(false, false, 100));
}

TEST_F(SemaphoreTest, semaphore_LessAcquires_noTryAcquire_ManyOperations) {
    ASSERT_TRUE(test_using(false, false, 10000));
}