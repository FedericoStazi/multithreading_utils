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
            update<void>(value);
    }

    /// @brief Acquires a resource.
    /// Acquires a single resource unit (decreases counter).
    /// Waits if no more resources can be acquired.
    /// \param value Number of resources to be acquired (1 by default).
    void acquire(int value = 1) {
        assert(inside_bounds(value));
        if (value != 0)
            update<void>(-value);
    }

    /// @brief Tries releasing a resource.
    /// Releases a single resource unit (increases counter).
    /// Does not wait if no more resources can be released.
    /// \param value Number of resources to be released (1 by default).
    /// \return True if the resource can be released, false otherwise
    bool try_release(int value = 1) {
        if (value == 0) return true;
        assert(inside_bounds(value));
        return update<bool>(value);
    }

    /// @brief Tries acquiring a resource.
    /// Acquires a single resource unit (decreases counter).
    /// Does not wait if no more resources can be acquired.
    /// \param value Number of resources to be acquired (1 by default).
    /// \return True if the resource can be acquired, false otherwise
    bool try_acquire(int value = 1) {
        if (value == 0) return true;
        assert(inside_bounds(value));
        return update<bool>(-value);
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
    inline bool inside_bounds(int value) const { return 0 <= value and value <= max_count; }

    /// @brief Releases or acquires the semaphore's resources.
    /// Releases (value > 0) or acquires (value < 0) the semaphore's resources by value.
    /// This means increasing the counter by value (therefore decreasing it if value < 0).
    /// value must not be 0.
    /// \tparam ReturnType This can only be void or bool.
    /// If void, the method waits for the resources if they are not immediately available.
    /// If bool, the method tries acquiring the resources once and then returns.
    /// \param value The amount of resources to be released (value > 0) or released (value < 0).
    /// \return If ReturnType is bool, returns true if the resources were released or acquired, false otherwise.
    template<class ReturnType>
    ReturnType update(int value);

    /// Queues containing the threads waiting to release or acquire resources respectively.
    WaitingThreadsQueue release_queue, acquire_queue;

    /// Variable storing the number of available resources.
    std::atomic<int> counter = 0;

};

/// Object that can be used to control access to a common resource by multiple threads.
/// Internally stores a counter that can be increased or decreased by different threads.
/// The number of resources is always greater than or equal to 0.
/// @attention Threads are executed with a FIFO ordering.
/// This means that, for example, a thread waiting for many resources will be given prioirty over all threads coming
/// later, even those that need less resources and could immediately return.
using CountingSemaphore = Semaphore<>;

/// Object that can be used to control access to a common resource by multiple threads.
/// Internally stores a counter that can be increased or decreased by different threads.
/// The number of resources is always 0 or 1.
using BinarySemaphore = Semaphore<1>;

// Implementation of Semaphore's update method
template<std::ptrdiff_t max_count>
template<class ReturnType>
ReturnType Semaphore<max_count>::update(int value) {

    // Checking at compile time that ReturnType is void or bool
    static_assert(
        std::is_same<ReturnType, void>::value or std::is_same<ReturnType, bool>::value,
        "ReturnType can only be void (for waiting update) or bool (for non-waiting update)");
    
    // True if the method should wait for the resources.
    // False if it should only try once (and return false if resources are missing, true otherwise).
    constexpr bool waiting = std::is_void<ReturnType>::value;

    // References to the queues. These are used to avoid duplicate code.
    // (the sign of value only changes the queues accessed by this method)
    assert(value != 0);
    WaitingThreadsQueue &this_queue = (value > 0) ? release_queue : acquire_queue;
    WaitingThreadsQueue &other_queue = (value > 0) ? acquire_queue : release_queue;

    // WaitingThread created on the heap.
    // Otherwise the object created by a later killed thread would not be destroyed correctly.
    std::unique_ptr<WaitingThread> wt(new WaitingThread());
    
    // Variable returned if waiting is false.
    bool updated = true;

    // Push WaitingThread in the queue (in different ways depending on waiting).
    if constexpr (waiting)
        this_queue.push(wt.get());
    else if (!this_queue.push_if_empty(wt.get()))
        return false;

    {
        // Lock this thread's mutex
        std::unique_lock<std::mutex> lk(wt->mutex);

        // If waiting = true, check the condition and wait for a notification if it does not hold.
        // If waiting = false, check the condition once and set updated = false if it does not hold.
        while (!((inside_bounds(counter + value) and wt->first) or this_queue.is_clearing())) {
            if constexpr (!waiting) {
                updated = false;
                break;
            }
            wt->condition_variable.wait(lk);
        }

        // Increase counter if the condition holds.
        if (updated)
            counter += value;

        assert((inside_bounds(counter)) or this_queue.is_clearing());
    }

    // Pop the thread from the queue.
    this_queue.pop();

    // Notify the other queue.
    // (maybe the operation has released or acquired enough resources for a thread on the other queue)
    other_queue.notify_first();

    if constexpr (!waiting)
        return updated;

}

#endif //MULTITHREADING_UTILS_SEMAPHORE_H
