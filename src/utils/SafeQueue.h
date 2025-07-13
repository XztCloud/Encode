//
// Created by XztCloud on 2025/7/13.
//

#ifndef ENCODE_SAFEQUEUE_H
#define ENCODE_SAFEQUEUE_H

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace std;

template<typename T>
class SafeQueue {
private:
    queue<T> queue;
    mutex mutex;
    condition_variable cond;

public:
    virtual void push(const T& item) {
        lock_guard<std::mutex> lock(mutex); // 上锁
        queue.push(item);
        cond.notify_one();  // 通知其他线程
    }
    bool pop(T& item, bool block=false, int timeout_ms=0) {
        unique_lock<std::mutex> lock(mutex);
        if (block) {
            if (timeout_ms > 0) {
                this->cond.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                    [this] { return !this->queue.empty(); });
            } else {
                this->cond.wait(lock, [this] { return !this->queue.empty(); });
            }
        }
        else if (queue.empty()) {
            return false;
        }
        item = queue.front();
        queue.pop();
        return true;
    }
    bool empty() {
        lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }

    size_t size() {
        lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }
};




#endif //ENCODE_SAFEQUEUE_H
