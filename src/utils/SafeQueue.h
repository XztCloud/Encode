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
    void push(const T& item) {
        lock_guard<std::mutex> lock(mutex); // 上锁
        queue.push(item);
        cond.notify_one();  // 通知其他线程
    }
    bool pop(T& item) {
        unique_lock<std::mutex> lock(mutex);
        if (queue.empty()) {
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
};




#endif //ENCODE_SAFEQUEUE_H
