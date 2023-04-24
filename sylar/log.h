/**
 * @file log.h
 * @brief 日志模块封装
 * @author Yjyu
 * @email 1137658927@qq.com
 * @date 2023-4-24
 */

#ifndef __SYLAR_LOG_H__
#define __SYLAR_LOG_H__

#include <string>
#include <stdint.h>
#include <memory>
#include <list>

namespace sylar {

/**
 * @brief 日志事件: 将每次的日志信息封装成日志事件
 */

class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;  // 使用智能指针管理该类对象
    LogEvent();
private:
    // 日志写入的日志文件名
    const char * m_file = nullptr;
    // 日志写入的日志文件的行号
    int32_t m_line = 0;

    // 线程ID   ?
    uint32_t m_thread_id = 0;
    // 协程ID   ?
    uint32_t m_fiber_id = 0;

    // 程序启动到现在的毫秒数
    uint32_t m_elapse = 0;  
    // 时间戳
    uint64_t m_time = 0;
    // 日志内容
    std::string m_content;
};

/**
 * @brief 日志级别
 */
class LogLevel {
public:
    /**
     * @brief 日志级别枚举
     */
    enum Level {
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5,
    };

};

/**
 * @brief 日志格式器: 不同目的地可能有不同的日志格式
 */
class LogFormatter {
public:
   typedef std::shared_ptr<LogAppender> ptr;

   std::string format(LogEvent::ptr event);
};

/**
 * @brief 日志输出地
 */ 
class LogAppender {
public:
    typedef std::shared_ptr<LogAppender> ptr;  // 使用智能指针管理该类对象
    virtual ~LogAppender() {}
    
    void Log(LogLevel::Level levle, LogEvent::ptr event);
    
private:
    // 日志级别
    LogLevel::Level m_level;
};


/**
 * @brief 日志器
 */
class Logger {
public:
    typedef std::shared_ptr<LogEvent> ptr;

    Logger();

    /**
     * @brief 构造函数
     * @param[in] name 日志器名称
     */
    Logger(const std::string& name = "root");
    /**
     * @brief 写日志
     * @param[in] level 日志级别
     * @param[in] event 日志事件
     */
    void log(LogLevel::Level level, const LogEvent::ptr event);


    /**
     * @brief 写debug级别日志
     * @param[in] event 日志事件
     */
    void debug(LogEvent::ptr event);

    /**
     * @brief 写info级别日志
     * @param[in] event 日志事件
     */
    void info(LogEvent::ptr event);

    /**
     * @brief 写warn级别日志
     * @param[in] event 日志事件
     */
    void warn(LogEvent::ptr event);

    /**
     * @brief 写error级别日志
     * @param[in] event 日志事件
     */
    void error(LogEvent::ptr event);

    /**
     * @brief 写fatal级别日志
     * @param[in] event 日志事件
     */
    void fatal(LogEvent::ptr event);

    /**
     * @brief 添加日志目标
     * @param[in] appender 日志目标
     */
    void addAppender(LogAppender::ptr appender);
    /**
     * @brief 删除日志目标
     * @param[in] appender 日志目标
     */
    void delAppender(LogAppender::ptr appender);

    /**
     * @brief 返回日志级别
     */
    LogLevel::Level getLevel() const { return m_level}
    /**
     * @brief 设置日志级别
     */
    void setLevel(LogLevel::Level val) { m_level = val}


private:
    /// 日志名称
    std::string m_name;
    /// 日志级别: 只有满足日志级别的日志才会被输出
    LogLevel::Level m_level;
    /// 日志目标集合
    std::list<LogAppender::ptr> m_appenders;

};


/**
 * @brief 输出到控制台的Appender
 */
class StdoutLogAppender : public LogAppender {

};

/**
 * @brief 输出到文件的Appender
 */
class FileLogAppender : public LogAppender {

};


}

#endif