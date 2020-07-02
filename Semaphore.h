//
// Created by federico on 01/07/2020.
//

#ifndef MULTITHREADING_UTILS_SEMAPHORE_H
#define MULTITHREADING_UTILS_SEMAPHORE_H


#include <atomic>
#include <mutex>
#include <iostream>
#include <condition_variable>

template<std::ptrdiff_t max_count = PTRDIFF_MAX>
class Semaphore {

public:

    ~Semaphore() {
        destroying = true;
        rel_cv.notify_all();
        acq_cv.notify_all();
    }

    /// increment internal counter
    void release(std::ptrdiff_t update = 1) {

        std::unique_lock<std::mutex> rel_lk(release_mutex);
        rel_cv.wait(rel_lk, [&] { return counter + update <= max() or destroying; });

        {
            std::lock_guard<std::mutex> acq_lk(acquire_mutex);
            counter += update;
            assert(counter <= max() or destroying);
        }

        acq_cv.notify_one();

    }

    /// decrement internal counter
    void acquire() {

        std::unique_lock<std::mutex> acq_lk(acquire_mutex);
        acq_cv.wait(acq_lk, [&] { return counter - 1 >= 0 or destroying; });

        {
            std::lock_guard<std::mutex> rel_lk(release_mutex);
            counter--;
            assert(counter >= 0 or destroying);
        }

        rel_cv.notify_one();

    }

    bool try_acquire() {

        std::unique_lock<std::mutex> acq_lk(acquire_mutex);
        std::lock_guard<std::mutex> rel_lk(release_mutex);

        if (counter - 1 >= 0 or destroying) {
            counter--;
            rel_cv.notify_all();
            assert(counter >= 0 or destroying);
            return true;
        }

        return false;

    }


    static constexpr inline std::ptrdiff_t max() {
        return max_count;
    }

private:

    std::atomic<int> counter = 0;

    std::mutex release_mutex;
    std::mutex acquire_mutex;

    std::condition_variable rel_cv;
    std::condition_variable acq_cv;

    bool destroying = false;

};

using CountingSemaphore = Semaphore<>;
using BinarySemaphore = Semaphore<1>;

#endif //MULTITHREADING_UTILS_SEMAPHORE_H
