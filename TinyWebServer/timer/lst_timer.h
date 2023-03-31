/*
定时器：处理非活跃连接
由于非活跃连接占用了连接资源，严重影响服务器的性能，通过实现一个服务器定时器，处理这种非活跃连接，释放连接资源

- 服务器主循环为每一个连接创建一个定时器，并对每个连接进行定时
- 利用【升序时间链表容器】将所有定时器串联起来，若主循环接收到定时通知，则在链表中依次执行定时任务

- 利用alarm函数周期性地触发SIGALRM信号,该信号的信号处理函数利用管道通知主循环执行定时器链表上的定时任务.
- 使用统一事件源处理SIGALRM信号
*/

#ifndef LST_TIMER
#define LST_TIMER

#include <netinet/in.h>
#include <time.h>
#include "../log/log.h"


class util_timer;

// 定时器需要管理的资源类
struct client_data
{
    sockaddr_in address;
    int sockfd;
    
    util_timer *timer;
};

// 定时器类
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire;  // 超时时间（绝对时间）

    void (*cb_func)(client_data *); // 回调函数

    client_data *user_data; // 定时器管理的连接所用于的资源

    util_timer *prev;
    util_timer *next;
};

// 定时器容器类——基于升序链表实现
class sort_timer_lst
{
public:
    sort_timer_lst() : head( NULL ), tail( NULL ) {}

    ~sort_timer_lst()
    {   // 销毁链表
        util_timer *tmp = head;
        while (tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    // 向容器中添加定时器
    void add_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }

        // 第一个定时器，则直接添加
        if (!head)
        {
            head = tail = timer;
            return;
        }

        // 根据过期时间排序
        //如果新的定时器超时时间小于当前头部结点, 直接作为头结点
        if (timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }

    // 调整定时器，任务发生变化时，调整定时器在链表中的位置
    // 客户端在设定时间内有数据收发,则当前时刻对该定时器重新设定时间
    void adjust_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }

        util_timer *tmp = timer->next;
        // 当前定时器的超时时间仍然比后一个要先，因此不用调整
        if (!tmp || (timer->expire < tmp->expire))
        {
            return;
        }
        
        //被调整定时器是链表头结点，将定时器取出，重新插入
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        }
        else     //被调整定时器在内部，将定时器取出，重新插入
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    // 删除定时器
    void del_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }

        // 链表中只有一个定时器，需要删除该定时器
        if ((timer == head) && (timer == tail))
        {
            delete timer;
            head = NULL;
            tail = NULL;
            return;
        }
        
        // 要删除的定时器是头结点
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        // 要删除的定时器是尾结点
        if (timer == tail)
        {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        // 要删除的定时器中链表中间结点
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    // 定时任务处理函数
    // 遍历定时器升序链表容器，从头结点开始依次处理每个定时器，
    // 若当前时间大于定时器超时时间，即找到了到期的定时器，执行回调函数，然后将它从链表中删除，然后继续遍历
    // 直到遇到尚未到期的定时器
    void tick()
    {
        if (!head)
        {
            return;
        }

        LOG_INFO("%s", "timer tick");
        Log::get_instance()->flush();

        // 获取当前时间
        time_t cur = time(NULL);

        util_timer *tmp = head;
        while (tmp)
        {
            if (cur < tmp->expire)
            {
                break;
            }
            // 调用定时器的回调函数
            tmp->cb_func(tmp->user_data);

            // 继续遍历
            head = tmp->next;   // 重置头结点
            if (head)
            {
                head->prev = NULL;
            }
            delete tmp;
            tmp = head;
        }
    }
private:

    // 调整链表内部结点
    // add_timer()和adjust_timer()都会调用
    void add_timer(util_timer *timer, util_timer *lst_head)
    {
        util_timer *prev = lst_head;
        util_timer *tmp = prev->next;
        // 找到新timer应该插入的位置,并插入
        while (tmp)
        {
            if (timer->expire < tmp->expire)
            {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        
        // 遍历完发现，目标定时器需要放到尾结点
        if (!tmp)
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }

private:
    // 双向链表
    util_timer *head;
    util_timer *tail;

};
#endif
