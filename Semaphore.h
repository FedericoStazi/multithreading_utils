//
// Created by federico on 01/07/2020.
//

#ifndef MULTITHREADING_UTILS_SEMAPHORE_H
#define MULTITHREADING_UTILS_SEMAPHORE_H


#include <atomic>
#include <mutex>
#include <iostream>

template<std::ptrdiff_t max_count = PTRDIFF_MAX>
class Semaphore {

public:

    /// increment internal counter
    void release(std::ptrdiff_t update = 1) {
        // TODO busy wait, must be improved!
        int temp_counter;
        do {
            temp_counter = counter;
        } while (
            temp_counter + update > max() or
            !counter.compare_exchange_strong(temp_counter, temp_counter + update));
    }

    /// decrement internal counter
    void acquire() {
        // TODO busy wait, must be improved!
        int temp_counter;
        do {
            temp_counter = counter;
        } while(
            temp_counter - 1 < 0 or
            !counter.compare_exchange_strong(temp_counter, temp_counter  - 1));
    }

    bool try_acquire() {
        int temp_counter = counter;
        return
            temp_counter - 1 >= 0 and
            counter.compare_exchange_strong(temp_counter, temp_counter  - 1);
    }

    static constexpr inline std::ptrdiff_t max() {
        return max_count;
    }

private:

    std::atomic<int> counter = 0;

};

using CountingSemaphore = Semaphore<>;
using BinarySemaphore = Semaphore<1>;

#endif //MULTITHREADING_UTILS_SEMAPHORE_H
