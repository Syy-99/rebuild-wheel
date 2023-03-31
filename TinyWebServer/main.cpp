#include <libgen.h>    // basename()
#include <signal.h>    // sigaction
#include <cstring>      // memset
#include <cassert>      // assert()
// socket编程API
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/epoll.h>  // epoll实现IO复用API

#include "./log/log.h"
#include "./CGImysql/sql_connection_pool.h"
#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"
#include "./timer/lst_timer.h"

#define SYNLOG          // 同步写日志
// #define ASYNLOG //异步写日志


//#define listenfdET //边缘触发非阻塞
#define listenfdLT //水平触发阻塞

#define MAX_FD 65536    //最大文件描述符
#define MAX_EVENT_NUMBER 10000 //最大事件数
#define TIMESLOT 5             //最小超时单位

//这三个函数在http_conn.cpp中定义
extern int addfd(int epollfd, int fd, bool one_shot);
extern int setnonblocking(int fd);

// 实现统一信号源处理的管道（传递信号类型)
static int pipefd[2]; 

// 初始化定时器容器
static sort_timer_lst timer_lst;

// 用来保存epoll的数据结构的标志
static int epollfd = 0;


//信号处理函数
void sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);     // 将信号的类型发送到管道中，进行统一信号源处理
    errno = save_errno;
}


// 设置信号处理函数
// param 1: 需要处理的信号
// param 2: 相应的信号处理函数
// param 3: 设置该信号中断系统调用后是否能恢复该系统调用
void addsig(int sig, void(handler)(int), bool restart = true) 
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;   // 设置信号处理函数

    // 设置是否重启被信号中断的系统调用
    if(restart) 
        sa.sa_flags |= SA_RESTART;

    sigfillset(&sa.sa_mask);    // 在信号处理时，挂起所有的信号

    assert(sigaction(sig, &sa, NULL) != -1);

}

// 定时器回调函数，删除非活动连接在socket上的注册事件，并关闭
void cb_func(client_data *user_data)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);   // 删除定期器管理的连接
    assert(user_data);
    
    close(user_data->sockfd);
    
    http_conn::m_user_count--;
    
    LOG_INFO("close fd %d", user_data->sockfd);
    Log::get_instance()->flush();
}

// ?????
void show_error(int connfd, const char *info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char **argv)
{

// 日志系统和服务器程序一起启动
#ifdef ASYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 8); // 异步日志模型
#endif

#ifdef SYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 0); // 同步日志模型
#endif

    // 未指定监听的端口号
    if (argc <= 1)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    int listen_port = atoi(argv[1]);    // 获得需要监听的端口

    // 服务器需要忽略SIGPIPE信号（默认行为是终止进程）
    addsig(SIGPIPE, SIG_IGN);

    // 创建MySQL数据库连接池
    connection_pool *connPool = connection_pool::GetInstance();
    connPool->init("localhost", "syy", "", "yourdb", 3306, 8);  // 初始化数据库连接池（8个连接）

    // 创建线程池
    threadpool<http_conn> *pool = nullptr;
    try
    {
        pool = new threadpool<http_conn>(connPool);   // 指定该线程池中从哪个连接池中获取连接
    }
    catch (...)
    {
        return 1;
    }

    http_conn *users = new http_conn[MAX_FD];   // 创建MAX_FD个http类对象
    assert(users);
    // 从数据库中，将所有的用户信息读取到内存中
    users->initmysql_result(connPool);  // 这个和具体哪个HTTP连接没关系，因此直接调用即可

    // 创建监听套接字
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    // 构造地址信息
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(listen_port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);    // INADDR_ANY本机IP地址

    // 配置套接字选项——SO_REUSEADDR： 处于Time-wait端口可以快速重用
    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    // 监听套接字绑定到对应的端口
    int ret = 0;
    ret = bind(listenfd,(struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    // 开始监听
    ret = listen(listenfd, 5);
    assert(ret >= 0);

    // 利用epoll进行IO复用
    // 创建内核事件表
    epollfd = epoll_create(5);
    assert(epollfd != -1); 

    // 将监听套接字注册到内核事件表中，并设置其为非阻塞
    addfd(epollfd, listenfd, false); 
    http_conn::m_epollfd = epollfd;

    epoll_event events[MAX_EVENT_NUMBER];   // 在调用epoll_wait时用来保存发生事件的fd


    // 创建管道， 这里的管道是为了实现统一事件源
    // 管道写端写入信号值，管道读端通过I/O复用系统监测读事件
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    addfd(epollfd, pipefd[0], false);   // 将管道的读事件注册到内核事件表中？？？
    setnonblocking(pipefd[1]);          // 设置管道写端非阻塞

    // 处理SIGALRM和SIGTERM信号，它们的信号处理函数只是将信号的类型写入管道
    addsig(SIGALRM, sig_handler, false);    
    addsig(SIGTERM, sig_handler, false);

    // 管理连接的定时器类
    client_data *users_timer = new client_data[MAX_FD];

    // 循环条件
    bool stop_server = false;   // 是否停止服务器进程的运行

    //超时标志
    bool timeout = false;
    assert(ret != -1);


    while(!stop_server) 
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR){
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        // 轮询文件描述符
        for (int i = 0; i < number; i++) 
        {
            int sockfd = events[i].data.fd;

            if (sockfd == listenfd) 
            {   // 监听套接字有可读事件，说明有新的连接到来

                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
#ifdef listenfdLT
                // 建立连接套接字
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                if (connfd < 0) {
                    LOG_ERROR("%s:errno is:%d", "accept error", errno);
                    continue;   // 服务器程序继续执行
                }

                if (http_conn::m_user_count >= MAX_FD)
                {
                    show_error(connfd, "Internal server busy");
                    LOG_ERROR("%s", "Internal server busy");
                    continue;
                }
                users[connfd].init(connfd, client_address);

                //初始化client_data数据
                //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
                users_timer[connfd].address = client_address;
                users_timer[connfd].sockfd = connfd;

                util_timer *timer = new util_timer;
                timer->user_data = &users_timer[connfd];
                timer->cb_func = cb_func;   // 设置定时器到达时的回调函数

                time_t cur = time(NULL);
                timer->expire = cur + 3 * TIMESLOT;    // 设置超时时间

                users_timer[connfd].timer = timer;

                timer_lst.add_timer(timer);    // 将该定期器加入定期器容器中管理
#endif                

#ifdef listenfdET
                // 需要循环接收数据
                while (1)
                {
                    int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                    if (connfd < 0)
                    {
                        LOG_ERROR("%s:errno is:%d", "accept error", errno);
                        break;
                    }
                    if (http_conn::m_user_count >= MAX_FD)
                    {
                        show_error(connfd, "Internal server busy");
                        LOG_ERROR("%s", "Internal server busy");
                        break;
                    }
                    users[connfd].init(connfd, client_address);

                    //初始化client_data数据
                    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
                    users_timer[connfd].address = client_address;
                    users_timer[connfd].sockfd = connfd;
                    util_timer *timer = new util_timer;
                    timer->user_data = &users_timer[connfd];
                    timer->cb_func = cb_func;
                    time_t cur = time(NULL);
                    timer->expire = cur + 3 * TIMESLOT;
                    users_timer[connfd].timer = timer;
                    timer_lst.add_timer(timer);
                }
                continue;
#endif
            }

            // 处理异常事件
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                timer->cb_func(&users_timer[sockfd]);

                if (timer)
                {
                    timer_lst.del_timer(timer);
                }
            }

            // 处理信号
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) 
            {  // 如果是管道读端的文件描述符发生可读事件，说明有信号发生触发信号处理函数，并将信号类型通过写端写入
                int sig;
                char signals[1024];

                // 从管道的读端读出信号值
                //正常情况下，这里的ret返回值总是1，只有14和15两个ASCII码对应的字符
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) // 执行出错
                {
                    continue;  
                }
                else if (ret == 0)  // 非阻塞读
                {
                    continue;
                }else {

                    // 处理信号值对应的逻辑
                    for(int i = 0; i < ret; i++)
                    {
                        switch (signals[i])
                        {
                        case SIGALRM:
                            timeout =true;
                            break;
                        case SIGTERM:
                            stop_server = true;
                        }
                    }

                }
            }
            // 如果是连接套接字上的事件
            // 则处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN) // 对应的文件描述符可以读
            {
                util_timer *timer = users_timer[sockfd].timer;  // 获得该连接的定时器类
                
                // 一次性将数据读完
                if (users[sockfd].read_once())
                {
                    LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    Log::get_instance()->flush();

                    //若监测到读事件，将该事件放入请求队列，让工作线程可以处理
                    pool->append(users + sockfd);

                    //若有数据传输，则将定时器往后延迟3个单位
                    //并对新的定时器在链表上的位置进行调整
                    if (timer)
                    {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        LOG_INFO("%s", "adjust timer once");
                        Log::get_instance()->flush();
                        timer_lst.adjust_timer(timer);
                    }
                }
                else
                {   // ???为什们这里直接关闭连接呢?
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_lst.del_timer(timer);
                    }
                }
            }
            
            else if (events[i].events & EPOLLOUT)   // 对应的文件描述符可以写
            {
                util_timer *timer = users_timer[sockfd].timer;
                if (users[sockfd].write())  
                {
                    LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    Log::get_instance()->flush();

                    //若有数据传输，则将定时器往后延迟3个单位
                    //并对新的定时器在链表上的位置进行调整
                    if (timer)
                    {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        LOG_INFO("%s", "adjust timer once");
                        Log::get_instance()->flush();
                        timer_lst.adjust_timer(timer);
                    }
                }
                else
                {
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_lst.del_timer(timer);
                    }
                }
            }
        }
    }
    return 0;
}