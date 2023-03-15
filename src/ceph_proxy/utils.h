/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021. All rights reserved.
 */

#ifndef UTILS_H
#define UTILS_H

#include <mutex>
#include <atomic>
#include <condition_variable>

template <class type>
class ConcurrentQueue {
public:
    ConcurrentQueue() : head(new QueueItem), tail(head) {}
    ~ConcurrentQueue()
    {
        while (Dequeue(nullptr)) {}        // 释放未释放的queueItem空间，但是外界传入的 type 的空间无法释放
        if (head) {
            delete head;
        }
    }

    void EnqueueWithLock(type v)
    {
        struct QueueItem* qItem = new QueueItem(v);
        std::unique_lock<std::mutex> lock(queueMutex);
        tail->next = qItem;
        tail = tail->next;
    }

    void Enqueue(type v)
    {
        EnqueueWithLock(v);
    }

    // false when empty
    bool DequeueWithLock(type *v)
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        struct QueueItem* nextItem = head->next;
        if (nextItem == nullptr) {
            return false;
        }
        if (v) {
            *v = nextItem->value;
        }
        delete head;
        head = nextItem;
        return true;
    }

    bool Dequeue(type *v)
    {
        return DequeueWithLock(v);
    }

private:
    struct QueueItem {
        QueueItem() : next(nullptr) {}
        explicit QueueItem(type v): value(v), next(nullptr) {}

        type value;
        struct QueueItem* next;
        ~QueueItem()
        {
            next = nullptr;
        };
    };

    struct QueueItem* head;
    struct QueueItem* tail;
    std::mutex queueMutex;
};

class Semaphore {
public:
    explicit Semaphore(int c = 0) : count(c), semaphore(0) {}
    ~Semaphore() {}
    void PostOne()
    {
        int c = count.fetch_add(1, std::memory_order_release);
        if (c < 0) {
            semaphore.Post();
        }
    }
    void PostAll(int m)
    {
        for (int i = 0; i < m; i++) {
            PostOne();
        }
    }
    void Wait()
    {
        int c = count.fetch_sub(1, std::memory_order_acquire);
        if (c < 1) {
            semaphore.Wait();
        }
    }
 
private:
    class Sem {
    public:
        explicit Sem(int c) : count(c) {}       // c >= 0 !
    
        void Post() noexcept
        {
            {
                std::unique_lock<std::mutex> lock(mtx);
                ++count;
            }
            cv.notify_one();
        }
    
        void Wait() noexcept
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&]() { return count != 0; });
            --count;
        }
    private:
        int count;
        std::mutex mtx;
        std::condition_variable cv;
    };
    std::atomic<int> count;
    int max;
    Sem semaphore;
};

#endif // UTILS_H