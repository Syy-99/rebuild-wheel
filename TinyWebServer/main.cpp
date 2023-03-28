#include <libgen.h>    // basename()
#include <signal.h>    // sigaction
#include <cstring>      // memset
#include <cassert>      // assert()

#include "./log/log.h"

#define SYNLOG // 同步写日志
// #define ASYNLOG //异步写日志


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


    return 0;
}