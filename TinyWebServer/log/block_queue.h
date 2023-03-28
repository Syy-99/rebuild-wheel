#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"

template <typename T>
class block_queue
{
public:
    // 设置队列的大小
    explicit block_queue(int max_size = 1000);

    // 清空队列
    void clear();

    // 判断队列是否满了
    bool full();

    // 判断队列是否为空
    bool empty();

    // 返回队首元素
    bool front(T &value);

    // 返回队尾元素
    bool back(T &value);

    // 返回目前队列的大小
    int size();

    // 返回队列的容量
    int max_size();


    // 向队列中添加元素
    bool push(const T &item);

    // 从队列中获取元素
    // 如果当前队列没有元素,将会等待条件变量
    bool pop(T &item);

    // 从队列中获取元素
    // 等等环境变量时，增加了超时处理：最多等于多少毫秒
    bool pop(T &item, int ms_timeout);

    ~block_queue();

private:
    locker block_queue_mutex_; // 每次操作阻塞队列都需要加锁
    cond block_queue_cond_;   // 阻塞队列类似生产者-消费者模型，需要锁+条件变量实现

    T *block_queue_ptr_;       // 实际的指针, 指向一个T类型的数组
    int block_queue_size_;     // 已经使用的大小
    int block_queue_max_size_; // 总大小

    // 模拟队列, 保存队头和队尾的位置索引
    int block_queue_front_;
    int block_queue_back_;
};
#endif
