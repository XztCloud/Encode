//
// Created by XztCloud on 2025/7/13.
//

#ifndef ENCODE_SAFELATESTQUEUE_H
#define ENCODE_SAFELATESTQUEUE_H

#include "SafeQueue.h"
using namespace std;

template<typename T>
class SafeLatestQueue : public SafeQueue<T> {
private:
    size_t max_size;  // queue size

public:
    explicit SafeLatestQueue(size_t max_size = 10) : max_size(max_size) {}
    void push_item(const T& item) {
        lock_guard<std::mutex> lock(this->mutex);   // 可重入锁
        this->push(item);

        while(this->queue.size() > max_size) {
            this->queue.pop();
        }
        this->cond.notify_one();    // 通知消费者
    }

};

#endif //ENCODE_SAFELATESTQUEUE_H
