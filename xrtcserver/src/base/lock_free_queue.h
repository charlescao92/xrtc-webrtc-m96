/**
 * @file lock_free_queue.h
 * @author charles
 * @brief 一个生产者，一个消费者的无锁队列的实现
 *         注意这里对于指针的操作是原子性的。
*/

#ifndef __BASE_LOCK_FREE_QUEUE_H_
#define __BASE_LOCK_FREE_QUEUE_H_

#include <atomic>

namespace xrtc {

template <typename T>
class LockFreeQueue {
public:
    LockFreeQueue() {
        first_ = divider_ = last_ = new Node(T());
        size_ = 0;
    }

    ~LockFreeQueue() {
        while (first_ != nullptr)
        {
            Node *temp = first_;
            first_ = first_->next;
            delete temp;
            temp = nullptr;
        }

        size_ = 0; 
    }

    void produce(const T& t) {
        last_->next = new Node(t);
        last_ = last_->next;
        ++size_;

        while(divider_ != first_) { // 说明有数据消费了，清理掉这个数据
            Node *temp = first_;
            first_ = first_->next;
            delete temp;
            temp = nullptr;
        }
    }

    bool consume(T *result) {
        if (divider_ != last_) {
            *result = divider_->next->value;
            divider_ = divider_->next;
            --size_;
            return true;
        }

        return false;
    }

    bool empty() const {
        return 0 == size_;
    }

    int size() const {
        return size_;
    }
    
private:
    struct Node {
        T value;
        Node *next;

        Node(const T& t) : value(t), next(nullptr) {}
    };

    Node *first_;
    Node *divider_;
    Node *last_;
    std::atomic<int> size_;
};

} // namespace xrtc

#endif // __BASE_LOCK_FREE_QUEUE_H_
