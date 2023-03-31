#include "threadpool.h"

// 线程池的创建
template <typename T>
threadpool<T>::threadpool( connection_pool *connPool, int thread_number, int max_requests) : 
thread_number_(thread_number), max_requests(max_requests), m_stop(false), thread_arr_(NULL),m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();

    // 创建保存线程对象的数组
    thread_arr_ = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();

    // 循环创建线程，并将工作线程按要求进行运行
    for (int i = 0; i < thread_number; ++i)
    {
        if (pthread_create(thread_arr_ + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        
        //将线程进行分离后，不用单独对工作线程进行回收
        // 一旦线程结束，会自动回收
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }

}

// 销毁线程池
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}


template <typename T>
bool threadpool<T>::append(T *request)
{
    queue_lock_.lock();

    // 如果请求队列已经满了，则返回false
    if (req_queue_.size() > max_requests)
    {
        queue_lock_.unlock();
        return false;
    }
    // 否则，将该请求信息插入队列中
    req_queue_.push_back(request);

    queue_lock_.unlock();

    queue_state_.post();    // 信号量加1
    return true;
}


template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;   // 传递线程池对象
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while (!m_stop)     // 注意这里是一个while循环，说明子线程会一直运行
    {
        queue_state_.wait();      // 信号量减1

        queue_lock_.lock();     // 加锁访问

        // 如果请求队列中没有请求，则不需要处理
        // 同上面的信号量减1的操作，应该不会到这个逻辑呀？
        if (req_queue_.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        // 从请求队列头部取出一个请求
        T *request = req_queue_.front();
        req_queue_.pop_front();     // 从队列中删除该任务的指针

        queue_lock_.unlock();

        // 如果这个请求是空的，则不需要处理
        if (!request)
            continue;

        connectionRAII mysqlcon(&request->mysql, m_connPool);  
        // request->mysql： 该请求对应的数据库连接
        
        // 工作线程一旦读取到HTTP请求报文，则会处理这个客户端请求
        request->process();
    }
}