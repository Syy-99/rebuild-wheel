/*
半同步/半反应堆线程池

- 使用一个工作队列完全解除了主线程和工作线程的耦合关系：主线程往工作队列中插入任务，工作线程通过竞争来取得任务并执行它。

- 利用同步IO模拟Reactor模式

*/


#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
using std::list;

template <typename T>
class threadpool
{
public:
    threadpool(connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();

    // 向所有线程共有的队列中插入请求
    bool append(T *request);

private:

    //工作线程运行的函数，它不断从工作队列中取出任务并执行之
    static void *worker(void *arg);
    
    void run();
private:

    pthread_t *thread_arr_;  // 描述线程池的数组
    int thread_number_;     // 线程池中的线程的数量
    
    list<T *> req_queue_;   // 请求队列
    int max_requests;       // 请求队列中允许的最大请求数量
    locker queue_lock_;     // 保护请求队列的互斥锁
    sem queue_state_;       // 队列的状态，即是否有任务需要处理
    
    bool m_stop;    // 是否结束线程?？？

    connection_pool *m_connPool;    // 线程需要获得数据库连接
};


#endif