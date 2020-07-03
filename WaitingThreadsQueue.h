//
// Created by federico on 03/07/2020.
//

#ifndef MULTITHREADING_UTILS_WAITINGTHREADSQUEUE_H
#define MULTITHREADING_UTILS_WAITINGTHREADSQUEUE_H


#include <mutex>
#include <condition_variable>
#include <queue>

/// Object used to store a single waiting thread.
struct WaitingThread {

    /// mutex used by condition_variable.
    std::mutex mutex;

    /// condition_variable that is notified when it is this thread's turn (i.e. it is the first in its queue).
    std::condition_variable condition_variable;

    /// boolean variable true if and only if this thread is the first in its queue.
    bool turn;

};

/// Object representing a queue of pointers to waiting threads.
/// All the methods provided are atomic.
/// The main invariants of this data structure are:
/// - turn true
/// - cv notified
class WaitingThreadsQueue {

public:

    /// Object destructor which deletes any threads that may still on the queues.
    /// It should be called after the threads using the semaphore have already been deleted.
    /// Only needed if the threads were killed while still waiting in the queue.
    ~WaitingThreadsQueue() {
        while (!queue.empty()) {
            delete queue.front();
            queue.pop();
        }
    }

    /// @brief Checks if the queue is currently being cleared
    /// Checks if the queue is currently being cleared.
    /// \return true if the queue is being cleared, false otherwise.
    [[nodiscard]] inline bool is_clearing() const { return clearing; }

    /// @brief Notifies the first thread on the queue.
    /// Atomically notifies (wakes up) the first thread of the queue.
    void notify_first() {
        std::lock_guard<std::mutex> lk(mutex);
        if (!queue.empty())
            queue.front()->condition_variable.notify_one();
    }

    /// @brief Removes first thread from the queue.
    /// Atomically removes the first thread from the queue and notifies (wakes up) the new first element.
    void pop() {
        std::lock_guard<std::mutex> lk(mutex);
        queue.pop();
        if (!queue.empty()) {
            queue.front()->turn = true;
            queue.front()->condition_variable.notify_one();
        }
    }

    /// @brief Pushes a thread in the queue.
    /// Atomically pushes a WaitingThread pointer in the queue and sets turn to true if it is the first.
    /// \param wt pointer to the WaitingThread object.
    void push(WaitingThread* wt) {
        std::lock_guard<std::mutex> tlk(mutex);
        if(queue.empty())
            wt->turn = true;
        queue.push(wt);
    }

    /// @brief Pushes a thread in the queue if it is empty.
    /// Atomically pushes a WaitingThread pointer in the queue if it is empty, and sets turn to true.
    /// \param wt pointer to the WaitingThread object.
    /// \return true if the thread was pushed, false otherwise.
    bool push_if_empty(WaitingThread* wt) {
        std::lock_guard<std::mutex> tlk(mutex);
        if (!queue.empty())
            return false;
        wt->turn = true;
        queue.push(wt);
        return true;
    }

    /// @brief Clears the queue.
    /// Clears the queue by waking up the threads one at a time and making them complete their operation
    /// despite the number of resources.
    /// It should be called before the threads are joined, and ensures correct clearing of the queue.
    inline void clear() {
        std::lock_guard<std::mutex> tlk(mutex);
        clearing = true;
        if (!queue.empty())
            queue.front()->condition_variable.notify_one();
    }

private:

    /// Queue used to store pointers to WaitingThreads
    std::queue<WaitingThread*> queue;

    /// Mutex protecting the queue
    std::mutex mutex;

    /// Boolean variable true if the queue is being cleared, false otherwise
    bool clearing = false;

};



#endif //MULTITHREADING_UTILS_WAITINGTHREADSQUEUE_H
