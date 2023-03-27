// 线程同步机制包装类：通过RAII机制空间锁资源
// 线程同步的方法：信号量；互斥量；条件变量

#ifndef LOCKER_H
#define LOCKER_H


#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 信号量RAII
class sem {
public:
    // 信号量初始值为0
    sem() {
        if (sem_init(&m_sem_,0, 0) != 0) {
            throw std::exception();
        }  
    }
    // 给予信号量一个初始值
    sem(int num): {
        if (sem_init(&m_sem_,0, num) != 0) {
            throw std::exception();
        }  
    } 
    // 确保销毁一个信号量
    ~sem()
    {
        sem_destroy(&m_sem_);  
    }

    // 信号量的PV操作
    bool wait()
    {
        // 减1，如果信号量的值为0，则该函数会被阻塞，直到这个信号量有非0值
        return sem_wait(&m_sem_) == 0; // 成功返回0，失败返回-1
    }
    bool post()
    {
        // 加1，当信号量值大于0时，其他阻塞在sem_wait的线程将被唤醒
        return sem_post(&m_sem_) == 0;
    }
private:
    sem_t m_sem_;
};

// 互斥量（锁）RAII
class locker {
public:
    // 初始化互斥锁
    locker()
    {
        if (pthread_mutex_init(&m_mutex_, NULL) != 0)
        {
            throw std::exception();
        }
    }
    // 确保销毁互斥锁
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex_);
    }
    // 互斥锁的加锁和解锁操作
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex_) == 0;
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex_) == 0;
    }

    // 获得互斥锁(条件变量可能使用)
    pthread_mutex_t *get()
    {
        return &m_mutex_;
    }

private:
    pthread_mutex_t m_mutex_;
};

// 条件变量RAII
class cond {
public:
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *m_mutex)     // 注意这里需要传入一个已经上锁的互斥锁
    {
        return pthread_cond_wait(&m_cond, m_mutex) == 0;
    }
    // 限时等待一个条件变量
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0;
    }

    // 
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;       // // 按调度策略和优先级，唤醒一个等待目标条件变量的线程
    }
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;    // 以广播的方式唤醒所有等待目标条件变量的线程
    }

private:
    pthread_cond_t m_cond;
};

#endif LOCKER_H
