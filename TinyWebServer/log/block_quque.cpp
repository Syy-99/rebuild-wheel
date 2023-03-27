#include "block_queue.h"

template <typename T>
block_queue<T>::block_queue(int max_size = 1000)
{

    if (max_size <= 0)
    {
        exit(-1);
    }

    block_queue_max_size_ = max_size;
    block_queue_ptr_ = new T[max_size];
    block_queue_size_ = 0;
    block_queue_front_ = -1;
    block_quque_back_ = -1;
}

template <typename T>
void block_queue<T>::clear()
{
    block_queue_mutex_.lock(); // 注意加锁（因为阻塞对象是一个共享对象）

    block_queue_size_ = 0;
    block_queue_front_ = -1;
    block_quque_back_ = -1;

    block_queue_mutex_.unlock();
}

template <typename T>
bool block_queue<T>::full()
{
    m_mutex.lock();
    if (m_size >= m_max_size)
    {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template <typename T>
bool block_queue<T>::empty()
{
    m_mutex.lock();
    if (0 == m_size)
    {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template <typename T>
bool block_queue<T>::front(T &value)
{
    m_mutex.lock();
    if (0 == m_size)
    {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_front];
    m_mutex.unlock();
    return true;
}

template <typename T>
bool block_queue<T>::back(T &value)
{
    m_mutex.lock();
    if (0 == m_size)
    {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_back];
    m_mutex.unlock();
    return true;
}
template <typename T>
int block_queue<T>::size()
{
    int tmp = 0;

    m_mutex.lock();
    tmp = m_size;

    m_mutex.unlock();
    return tmp;
}

template <typename T>
int block_queue<T>::max_size()
{
    int tmp = 0;

    m_mutex.lock();
    tmp = m_max_size;

    m_mutex.unlock();
    return tmp;
}

template <typename T>
bool block_queue<T>::push(const T &item)
{

    block_queue_mutex_.lock();

    // 如果阻塞队列已经满了，则无法插入，必须要其他线程读才行
    if (block_queue_size_ >= block_queue_max_size_)
    {
        block_queue_cond_.broadcast(); // 唤醒等待的线程
        block_queue_mutex_.unlock();
        return false;
    }

    // 队列未满，可以插入
    block_quque_back_ = (block_quque_back_ + 1) % block_queue_max_size_; // 循环使用
    block_queue_ptr_[block_quque_back_] = item;

    block_queue_size_++;

    block_queue_cond_.broadcast(); // 只要有内容，就尝试唤醒之前在等待队列的进程
    block_queue_mutex_.unlock();
    return true;
}

template <typename T>
bool block_queue<T>::pop(T &item)
{
    block_queue_mutex_.lock();

    // 如果调用pop时，队列为空，则加入等待队列，直到它被唤醒
    while (block_queue_size_ <= 0)
    {
        if (!block_queue_cond_.wait(block_queue_mutex_.get()))
        {
            block_queue_mutex_.unlock();
            return false;
        }
    }

    block_queue_front_ = (block_queue_front_ + 1) % block_queue_max_size_;
    item = block_queue_ptr_[block_queue_front_]; // 注意这里顺序，front先加1
    block_queue_size_--;
    block_queue_mutex_.unlock();
    return true;
}

template <typename T>
bool block_queue<T>::pop(T &item, int ms_timeout)
{
    struct timespec t = {0, 0};
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);

    block_queue_mutex_.lock();

    if (block_queue_size_ <= 0)
    {
        t.tv_sec = now.tv_sec + ms_timeout / 1000;
        t.tv_nsec = (ms_timeout % 1000) * 1000;

        if (!m_cond.timewait(m_mutex.get(), t))
        {
            m_mutex.unlock();
            return false;
        }
    }

    block_queue_front_ = (block_queue_front_ + 1) % block_queue_max_size_;
    item = block_queue_ptr_[block_queue_front_]; // 注意这里顺序，front先加1
    block_queue_size_--;
    block_queue_mutex_.unlock();
    return true;
}

template <typename T>
block_queue<T>::~block_queue()
{
    m_mutex.lock();

    if (m_array != NULL)
        delete[] m_array; // 释放空间

    m_mutex.unlock();
}
