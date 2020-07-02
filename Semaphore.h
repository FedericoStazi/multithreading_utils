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

// TODO: docs
template<std::ptrdiff_t max_count = PTRDIFF_MAX>
class Semaphore {

public:

    void clear() {

        {
            std::lock_guard<std::mutex> tlk(release_queue.get_mutex());
            release_queue.clear();
            release_queue.notify_first();
        }

        {
            std::lock_guard<std::mutex> tlk(acquire_queue.get_mutex());
            acquire_queue.clear();
            acquire_queue.notify_first();
        }

    }

    void release() {
        update<void>(+1, release_queue, acquire_queue);
    }

    void acquire() {
        update<void>(-1, acquire_queue, release_queue);
    }

    bool try_release() {
        return update<bool>(+1, release_queue, acquire_queue);
    }

    bool try_acquire() {
        return update<bool>(-1, acquire_queue, release_queue);
    }

    static constexpr inline std::ptrdiff_t max() {
        return max_count;
    }

private:

    struct WaitingThread {
        std::mutex m;
        std::condition_variable cv;
        bool turn = false;
    };

    class WaitingThreadsQueue {

    public:

        inline std::mutex& get_mutex() { return mutex; }

        inline bool is_clearing() { return clearing; }

        inline bool clear() { clearing = true; }

        bool empty() { return queue.empty(); }

        void push(WaitingThread* wt) { return queue.push(wt); }

        void pop() {
            queue.pop();
            if (!queue.empty())
                queue.front()->turn = true;
        }

        void notify_first() {
            if (!queue.empty())
                queue.front()->cv.notify_one();
        }

    private:

        std::queue<WaitingThread*> queue;
        std::mutex mutex;
        bool clearing = false;

    } release_queue, acquire_queue;

    std::atomic<int> counter = 0;

    template<class ReturnType> //ReturnType can only be void (for waiting update) or bool (for non-waiting update)
    ReturnType update(int value, WaitingThreadsQueue& this_queue, WaitingThreadsQueue& other_queue) {

        static_assert(
            std::is_same<ReturnType, void>::value or std::is_same<ReturnType, bool>::value,
            "ReturnType can only be void (for waiting update) or bool (for non-waiting update)");

        WaitingThread wt;
        bool updated = true;

        {
            std::lock_guard<std::mutex> tlk(this_queue.get_mutex());
            if(this_queue.empty())
                wt.turn = true;
            else if constexpr (!std::is_void<ReturnType>::value)
                return false;
            this_queue.push(&wt);
        }

        {
            std::unique_lock<std::mutex> lk(wt.m);
            while (!((counter + value >= 0 and counter + value <= max() and wt.turn) or this_queue.is_clearing())) {
                if constexpr (!std::is_void<ReturnType>::value) {
                    updated = false;
                    break;
                }
                wt.cv.wait(lk);
            }
            if (updated)
                counter += value;
            assert((counter >= 0 and counter <= max()) or this_queue.is_clearing());
        }

        {
            std::lock_guard<std::mutex> tlk(this_queue.get_mutex());
            this_queue.pop();
            this_queue.notify_first();
        }

        {
            std::lock_guard<std::mutex> tlk(other_queue.get_mutex());
            other_queue.notify_first();
        }

        if constexpr (!std::is_void<ReturnType>::value)
            return updated;

    }

};

using CountingSemaphore = Semaphore<>;
using BinarySemaphore = Semaphore<1>;

#endif //MULTITHREADING_UTILS_SEMAPHORE_H
