//
// Created by federico on 01/07/2020.
//

#ifndef MULTITHREADING_UTILS_SEMAPHORE_H
#define MULTITHREADING_UTILS_SEMAPHORE_H


#include <atomic>
#include <mutex>
#include <iostream>
#include <condition_variable>
#include <queue>
#include "WaitingThreadsQueue.h"

/// Object that can be used to control access to common resources by multiple threads.
/// Internally stores a counter that can be increased or decreased by different threads.
/// @attention Threads are executed with a FIFO ordering.
/// (a thread waiting for (possibly many) resources, will have priority on all other (possibly smaller) later requests)
/// \tparam max_count Upper limit of the counter's value. The lower limit is always 0.
template<std::ptrdiff_t max_count = PTRDIFF_MAX>
class Semaphore {

public:

    /// @brief Constructs the Semaphore.
    /// Constructs the Semaphore with a certain amount of initial resources.
    /// The constructor checks that the value of resources is a valid amount of resources.
    /// \param resources The number of initial resources, 0 by default.
    explicit Semaphore(int resources = 0) : counter(resources) {
        assert(inside_bounds(resources));
    }

    /// @brief Releases a resource.
    /// Releases a single resource unit (increases counter).
    /// Waits if no more resources can be released.
    /// \param value Number of resources to be released (1 by default).
    void release(int value = 1) {
        assert(inside_bounds(value));
        if (value != 0)
            update(value);
    }

    /// @brief Acquires a resource.
    /// Acquires a single resource unit (decreases counter).
    /// Waits if no more resources can be acquired.
    /// \param value Number of resources to be acquired (1 by default).
    void acquire(int value = 1) {
        assert(inside_bounds(value));
        if (value != 0)
            update(-value);
    }

    /// @brief Tries releasing a resource.
    /// Releases a single resource unit (increases counter).
    /// Does not wait if no more resources can be released.
    /// \param value Number of resources to be released (1 by default).
    /// \return True if the resource can be released, false otherwise
    bool try_release(int value = 1) {
        if (value == 0) return true;
        assert(inside_bounds(value));
        return try_update(value);
    }

    /// @brief Tries acquiring a resource.
    /// Acquires a single resource unit (decreases counter).
    /// Does not wait if no more resources can be acquired.
    /// \param value Number of resources to be acquired (1 by default).
    /// \return True if the resource can be acquired, false otherwise
    bool try_acquire(int value = 1) {
        if (value == 0) return true;
        assert(inside_bounds(value));
        return try_update(-value);
    }

    /// @brief Clears the semaphore's queues.
    /// Clears the semaphore's queues of waiting threads and sets the number of resources (counter) to 0.
    /// Can be called before joining and destroying threads that are still waiting on some mutexes.
    void clear() {
        release_queue.clear();
        acquire_queue.clear();
        counter = 0;
    }

    /// @brief Returns the maximum number of resources
    /// Returns the maximum number of resources
    /// \return Maximum number of resources
    static constexpr inline std::ptrdiff_t max() { return max_count; }

private:

    /// Checks if value is between 0 and max_count (0 <= value and value <= max_count).
    /// \param value Value to be checked.
    /// \return True if value is between 0 and max_count, false otherwise.
    inline bool inside_bounds(int value) const { return 0 <= value and value <= max(); }

    /// @brief Releases or acquires the semaphore's resources, waiting if not available.
    /// Releases (value > 0) or acquires (value < 0) the semaphore's resources by value.
    /// This means increasing the counter by value (therefore decreasing it if value < 0).
    /// (value must not be 0).
    /// This method waits for the resources if they are not immediately available.
    /// \param value The amount of resources to be released (value > 0) or acquired (value < 0).
    void update(int value);

    /// @brief Releases or acquires the semaphore's resources, returning false if not available.
    /// Releases (value > 0) or acquires (value < 0) the semaphore's resources by value.
    /// This means increasing the counter by value (therefore decreasing it if value < 0).
    /// (value must not be 0).
    /// This method tries acquiring the resources once and then returns.
    /// \param value The amount of resources to be released (value > 0) or acquired (value < 0).
    /// \return Returns true if the resources were released or acquired, false otherwise.
    bool try_update(int value);

    /// Queues containing the threads waiting to release or acquire resources respectively.
    WaitingThreadsQueue release_queue, acquire_queue;

    /// Variable storing the number of available resources.
    std::atomic<int> counter = 0;

};

/// Object that can be used to control access to a common resource by multiple threads.
/// Internally stores a counter that can be increased or decreased by different threads.
/// The number of resources is always greater than or equal to 0.
/// @attention Threads are executed with a FIFO ordering.
/// This means that, for example, a thread waiting for many resources will be given priority over all threads coming
/// later, even those that need less resources and could immediately return.
using CountingSemaphore = Semaphore<>;

/// Object that can be used to control access to a common resource by multiple threads.
/// Internally stores a counter that can be increased or decreased by different threads.
/// The number of resources is always 0 or 1.
using BinarySemaphore = Semaphore<1>;

// Implementation of Semaphore's update method
template<std::ptrdiff_t max_count>
void Semaphore<max_count>::update(int value) {

    // References to the queues. These are used to avoid duplicate code.
    // (the sign of value only changes the queues accessed by this method)
    WaitingThreadsQueue &this_queue = (value > 0) ? release_queue : acquire_queue;
    WaitingThreadsQueue &other_queue = (value > 0) ? acquire_queue : release_queue;

    {
        auto g = this_queue.waiting_guard([&](){ return inside_bounds(counter + value); });
        counter += value;
    }

    // Notify the other queue.
    // (maybe the operation has released or acquired enough resources for a thread on the other queue)
    other_queue.notify();

}

// Implementation of Semaphore's update method
template<std::ptrdiff_t max_count>
bool Semaphore<max_count>::try_update(int value) {

    // References to the queues. These are used to avoid duplicate code.
    // (the sign of value only changes the queues accessed by this method)
    WaitingThreadsQueue &this_queue = (value > 0) ? release_queue : acquire_queue;
    WaitingThreadsQueue &other_queue = (value > 0) ? acquire_queue : release_queue;

    bool can_update;

    {
        auto g = this_queue.returning_guard([&]() { return inside_bounds(counter + value); });
        can_update = g.can_update();
        if (can_update)
            counter += value;
    }

    // Notify the other queue.
    // (maybe the operation has released or acquired enough resources for a thread on the other queue)
    other_queue.notify();

    return can_update;

}

#endif //MULTITHREADING_UTILS_SEMAPHORE_H
