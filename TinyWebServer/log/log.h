#ifndef LOG_H
#define LOG_H

#include <cstdio>
#include "../lock/locker.h"
#include "block_queue.h"
#include <string>
using std::string;

// 1. 使用单例模式生成的日志对象
// 2. 支持同步日志(默认)和异步日志：异步日志通过实现一个阻塞队列来实现
class Log {
public:

    // 单例模式需要禁止赋值构造函数
    Log(const Log &) = delete;
    Log& operator=(const Log &) = delete;

    // 单例模式：提供一个对外的获得对象实例的接口
    // C++11推荐使用静态局部变量的饿汉式方式
    static Log *get_instance() {
        static Log instance;
        return &instance;
    }

    // 初始化日志对象
    // 参数：Log文件路径；log buf大小；log file的最大条数; 阻塞队列的长度
    bool init(const char *file_name, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    // 对外提供写日志接口
    // 函数参数类似printf()
    void write_log(int levle, const char *format, ...);

    // 强制刷新
    void flush();


    static void * flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
        return nullptr;
    }

private:
    Log();

    ~Log();
    
    void async_write_log()     // 异步线程，将日志写到文件中
    {
        string single_log;
        //从阻塞队列中取出一个日志string，写入文件
        while (log_queue_->pop(single_log)) // 正常来说，这个while不会结束
        {
            log_file_mutex.lock();
            fputs(single_log.c_str(), log_file_fd_);
            log_file_mutex.unlock();
        }
    }

private:
    char log_dir_name_[128];    // 日志文件保存的路径
    char log_file_name_[128];    // 日志文件的名字
    
    FILE * log_file_fd_;        // 保存打开的日志文件的指针

    char * log_buf_;        // 指向日志缓冲的指针(缓冲在内存中)
    int log_buf_size_;      // 日志缓冲区大小（日志是磁盘文件，需要读取到内存进行读写）

    int log_max_line_;      // 日志文件最大条数
    int log_line_count_;     // 日志文件已经使用的条数记录

    bool log_is_async_;      // 判断是否设置为异步日志
    block_queue<string> *log_queue_; // 阻塞队列
    
    locker log_file_mutex;      // 日志对象是共享的资源，因此它的访问需要加锁
    
    int log_file_today;        // 日志文件按天创建，按天保存内容
};


// 提供四种日志的输出宏
#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__)     // ##__VA_ARGS__ 可变参数宏
#define LOG_INFO(format, ...) Log::get_instance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Log::get_instance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__)

#endif

