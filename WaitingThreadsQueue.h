//
// Created by federico on 03/07/2020.
//

#ifndef MULTITHREADING_UTILS_WAITINGTHREADSQUEUE_H
#define MULTITHREADING_UTILS_WAITINGTHREADSQUEUE_H


#include <mutex>
#include <condition_variable>
#include <queue>

/// Object representing a queue of waiting threads.
/// Threads using this object need to run some code,
/// and need a condition to always hold while the operations are performed
/// This is implemented using guards, objects that, while in scope, ensure that the condition always holds.
/// If the condition does not initially hold, both waiting and returning is supported.
/// In the waiting case, the guard waits for the condition to hold and for other threads to complete their operations.
/// In the returning case, the guard returns true if the condition holds and no thread is waiting, false otherwise.
class WaitingThreadsQueue {

private:

    /// Struct used to distinguish the two possible behaviours if the condition does not hold.
    struct OnFail {

        /// Wait for the condition to hold.
        struct Waiting;

        /// Returns immediately, true if the condition holds, false otherwise.
        struct Returning;
    };

    /// Object used to store a single waiting thread.
    struct WaitingThread {

        /// mutex used by condition_variable.
        std::mutex mutex;

        /// condition_variable that is notified when this thread becomes the first in its queue.
        std::condition_variable condition_variable;

        /// bool variable true if and only if this thread is the first in its queue.
        /// By default it is false.
        bool first = false;

    };

    /// Guard object protecting the queue.
    /// Can not be directly constructed, but can be constructed using WaitingThreadsQueue's methods.
    /// \tparam mode Can be OnFail::Waiting or OnFail::Returning.
    template<typename mode>
    class QueueGuard {

    public:

        /// Constructor that manages insertion in the queue and locking.
        QueueGuard(WaitingThreadsQueue& queue, std::function<bool()> condition) : queue(queue) {

            // mode can be OnFail::Waiting or OnFail::Returning.
            static_assert(std::is_same<mode, OnFail::Waiting>::value or std::is_same<mode, OnFail::Returning>::value);

            // create WaitingThread object
            wt = std::unique_ptr<WaitingThread>(new WaitingThread());

            // push in the queue
            if constexpr (std::is_same<mode, OnFail::Waiting>::value)
                queue.push(wt.get());
            else if (!queue.push_if_empty(wt.get())) {
                thread_pushed = false;
                return;
            }

            // lock the mutex protecting this thread
            lk = std::unique_lock<std::mutex>(wt->mutex);

            // wait on the condition (using a condition variable), or check if it holds and return
            if constexpr (std::is_same<mode, OnFail::Waiting>::value)
                wt->condition_variable.wait(lk, [&]() { return (condition() and wt->first) or queue.is_clearing(); });
            else
                condition_holds = (condition() and wt->first) or queue.is_clearing();
        }

        /// Destructor that manages removal from the queue and unlocking
        ~QueueGuard() {

            // remove from queue if it was pushed
            if (thread_pushed)
                queue.pop();
        }

        /// @brief Returns true if thread can update.
        /// After the returning guard was constructed, check if the condition holds.
        /// Must be used before updating because if the condition does not hold, the update should not happen.
        /// \return True if the condition holds or if this is a waiting thread, false otherwise.
        bool inline can_update() const {
            return condition_holds and thread_pushed;
        }

    private:

        /// True if the thread was pushed in the queue, false otherwise.
        bool thread_pushed = true;
        
        /// True if the condition holds, false otherwise (used by returning guards).
        bool condition_holds = true;
        
        /// Pointer to a WaitingThread object.
        std::unique_ptr<WaitingThread> wt;
        
        /// Reference to the queue creating this guard
        WaitingThreadsQueue& queue;
        
        /// Lock used on WaitingThread's mutex
        std::unique_lock<std::mutex> lk;

    };

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

    /// @brief Returns guard object waiting for the condition to hold
    QueueGuard<OnFail::Waiting> waiting_guard(std::function<bool()> pred) {
        return QueueGuard<OnFail::Waiting>(*this, pred);
    }

    /// @brief Returns guard object returning true if the condition holds, false otherwise.
    QueueGuard<OnFail::Returning> returning_guard(std::function<bool()> pred) {
        return QueueGuard<OnFail::Returning>(*this, pred);
    }

    /// @brief Notifies the queue.
    /// Atomically notifies the queue.
    /// Should be called when, after some operation, the condition may hold.
    void notify() {
        std::lock_guard<std::mutex> lk(mutex);
        if (!queue.empty())
            queue.front()->condition_variable.notify_one();
    }

    /// @brief Clears the queue.
    /// Clears the queue by waking up the threads one at a time and making them complete their operation
    /// despite the condition holding or not.
    /// It should be called before the threads are joined, and ensures correct clearing of the queue.
    inline void clear() {
        std::lock_guard<std::mutex> tlk(mutex);
        clearing = true;
        if (!queue.empty())
            queue.front()->condition_variable.notify_one();
    }

private:

    /// @brief Removes first thread from the queue.
    /// Atomically removes the first thread from the queue and notifies (wakes up) the new first element.
    void pop() {
        std::lock_guard<std::mutex> lk(mutex);
        queue.pop();
        if (!queue.empty()) {
            queue.front()->first = true;
            queue.front()->condition_variable.notify_one();
        }
    }

    /// @brief Pushes a thread in the queue.
    /// Atomically pushes a WaitingThread pointer in the queue and sets first to true if it is the first.
    /// \param wt pointer to the WaitingThread object.
    void push(WaitingThread* wt) {
        std::lock_guard<std::mutex> tlk(mutex);
        if(queue.empty())
            wt->first = true;
        queue.push(wt);
    }

    /// @brief Pushes a thread in the queue if it is empty.
    /// Atomically pushes a WaitingThread pointer in the queue if it is empty, and sets first to true.
    /// \param wt pointer to the WaitingThread object.
    /// \return true if the thread was pushed, false otherwise.
    bool push_if_empty(WaitingThread* wt) {
        std::lock_guard<std::mutex> tlk(mutex);
        if (!queue.empty())
            return false;
        wt->first = true;
        queue.push(wt);
        return true;
    }

    /// @brief Checks if the queue is currently being cleared
    /// Checks if the queue is currently being cleared.
    /// Only used for assertions!
    /// \return true if the queue is being cleared, false otherwise.
    [[nodiscard]] inline bool is_clearing() const { return clearing; }

    /// Queue used to store pointers to WaitingThreads
    std::queue<WaitingThread*> queue;

    /// Mutex protecting the queue
    std::mutex mutex;

    /// Boolean variable true if the queue is being cleared, false otherwise
    bool clearing = false;

};

#endif //MULTITHREADING_UTILS_WAITINGTHREADSQUEUE_H
