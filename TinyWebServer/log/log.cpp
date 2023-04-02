
#include "log.h"
#include <cstring>
#include <ctime>
#include <cstdarg>
using std::string;

Log::Log()
{
    log_line_count_ = 0;
    log_is_async_ = false;      // 默认使用同步日志
}

Log::~Log() {
    if (log_file_fd_ != nullptr)
        fclose(log_file_fd_);
}

//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size) {

    //如果设置了max_queue_size,则设置为异步
    if (max_queue_size >= 1) {
        log_is_async_ = true;
       
        // 设置阻塞队列
        log_queue_ = new block_queue<string>(max_queue_size);
        
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL);

    }

    // 初始化日志缓冲
    log_buf_size_ = log_buf_size;
    log_buf_ = new char[log_buf_size_];
    memset(log_buf_, 0, log_buf_size_);

    log_max_line_ = split_lines;

    // char *strrchr(const char *str, int c) 
    // 在参数 str 所指向的字符串中搜索最后一次出现字符 c（一个无符号字符）的位置。
    const char *p = strrchr(file_name, '/');   
    char log_file_full_name[256] = {0};

    // 获得程序执行时系统的时间
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    if (p == nullptr) {     // 如果创建的文件不包含目录名
        // int snprintf(char *str, size_t size, const char *format, ...) 
        // 可变参数(...)按照 format 格式化成字符串，并将字符串复制到 str 中，size 为要写入的字符的最大数目，超过 size 会被截断，最多写入 size-1 个字符。
        snprintf(log_file_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);

        strcpy(log_file_name_, file_name);
    } else {        // 如果创建的文件包含目录名
        strcpy(log_file_name_, p + 1);
        strncpy(log_dir_name_, file_name, p - file_name + 1);
        snprintf(log_file_full_name, 255, "%s%d_%02d_%02d_%s", log_dir_name_, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_file_name_);
    }

    strcpy(log_file_name_, log_file_full_name);  // 保存日志文件名

    log_file_fd_ = fopen(log_file_full_name, "a");  // "a": 追加到一个文件。写操作向文件末尾追加数据。如果文件不存在，则创建文件。
    if (log_file_fd_ == NULL)
    {
        return false;
    }

    log_file_today = my_tm.tm_mday; // 记录该日志在哪天创建

    return true;
}

// 写日志到日志文件
void Log::write_log(int level, const char *format,...) {

    // 获得当前的时间的微妙
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;

    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 因为会对日志文件的属性进行访问/更新，因此需要加锁
    log_file_mutex.lock();

    log_line_count_++;  // 日志已经写入的记录数

    // 判断是否需要新建log file
    if (log_file_today != my_tm.tm_mday || log_line_count_ % log_max_line_ == 0) {
        // 关闭之前的日志文件（关闭之前刷新它的缓冲区）
        fflush(log_file_fd_);
        fclose(log_file_fd_);

        char new_log_file_name_prefix[16] = {0};
        snprintf(new_log_file_name_prefix, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        char new_log_file_path[256] = {0};
        if (log_file_today != my_tm.tm_mday) {         // 不同天的日志，则修改前缀
            snprintf(new_log_file_path, 255, "%s%s%s", log_dir_name_, new_log_file_name_prefix, log_file_name_);
            log_file_today = my_tm.tm_mday;
            log_line_count_ = 0;
        } else {       // 同一天的日志太多，则增加后缀
             snprintf(new_log_file_path, 255, "%s%s%s.%lld", log_dir_name_, new_log_file_name_prefix, log_file_name_, log_line_count_ / log_max_line_);
        }
        // 创建新的日志文件，并将日志写入定位到该文件
        log_file_fd_ = fopen(new_log_file_path,"a");
    }

    log_file_mutex.unlock();

    // 开始构造日志的完整输出的内容

    // 输出不同的日志等级
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }

    // 获得可变参数列表
    va_list valst;
    va_start(valst, format);

    string log_str;     // 格式转换用

    log_file_mutex.lock();

    //写入的具体时间内容格式：年-月-日 时：分：秒：毫秒 等级
    int n = snprintf(log_buf_, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    // vsnprintf(): 将格式化的数据写入字符串
    int m = vsnprintf(log_buf_ + n, log_buf_size_ - 1, format, valst);
    log_buf_[n + m] = '\n';     // 添加换行符
    log_buf_[n + m + 1] = '\0';        // vsnprintf()不会添加，需要手动加\0

    log_str = log_buf_;

    log_file_mutex.unlock();

    
    if (log_is_async_ && !log_queue_->full())   // 异步日志先写到阻塞队列中
    {
        log_queue_->push(log_str);
    }
    else    // 同步日志直接写到磁盘中
    {
        log_file_mutex.lock();
        fputs(log_str.c_str(), log_file_fd_);
        log_file_mutex.unlock();
    }

    va_end(valst);  // 释放
    
}

void Log::flush()
{
     log_file_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(log_file_fd_);
    log_file_mutex.unlock();
}