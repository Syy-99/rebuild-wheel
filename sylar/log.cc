#include "log.h"

namespace sylar {


Logger::Logger(const std::string & name) : m_name(name) {

}

void Logger::addAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    if(!appender->getFormatter()) {
        MutexType::Lock ll(appender->m_mutex);
        appender->m_formatter = m_formatter;
    }
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    for(auto it = m_appenders.begin();
            it != m_appenders.end(); ++it) {
        if(*it == appender) {
            m_appenders.erase(it);
            break;
        }
    }
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    // if(level >= m_level) {
    //     auto self = shared_from_this();
    //     MutexType::Lock lock(m_mutex);
    //     if(!m_appenders.empty()) {
    //         for(auto& i : m_appenders) {
    //             i->log(self, level, event);
    //         }
    //     } else if(m_root) {
    //         m_root->log(level, event);
    //     }
    // }
}

void Logger::debug(LogEvent::ptr event) {
    log(LogLevel::DEBUG, event);
}

void Logger::info(LogEvent::ptr event) {
    log(LogLevel::INFO, event);
}

void Logger::warn(LogEvent::ptr event) {
    log(LogLevel::WARN, event);
}

void Logger::error(LogEvent::ptr event) {
    log(LogLevel::ERROR, event);
}

void Logger::fatal(LogEvent::ptr event) {
    log(LogLevel::FATAL, event);
}
}