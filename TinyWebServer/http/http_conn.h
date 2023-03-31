/*
http连接处理类

- 浏览器端发出http连接请求，主线程创建http对象接收请求并将所有数据读入对应buffer，将该对象插入任务队列，工作线程从任务队列中取出一个任务进行处理

- 工作线程取出任务后，调用process_read函数，通过主、从状态机对请求报文进行解析。

- 解析完之后，跳转do_request函数生成响应报文，通过process_write写入buffer，返回给浏览器端
*/

#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H


#include <sys/socket.h>
#include <sys/epoll.h>  // epoll实现IO复用API
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include "../CGImysql/sql_connection_pool.h"
#include "../log/log.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;       // 设置读取文件的名称m_real_file大小
    static const int READ_BUFFER_SIZE = 2048;  // 设置读缓冲区m_read_buf大小
    static const int WRITE_BUFFER_SIZE = 1024; // 设置写缓冲区m_write_buf大小

    // 主状态机的三种状态，对应HTTP报文的三部分
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    // 从状态机读取行的状态
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN   // 读取的行不完整
    };

    // HTTP请求的处理结果
    enum HTTP_CODE
    {
        NO_REQUEST,     // 请求不完整，需要继续读取请求报文数据->继续监听
        GET_REQUEST,    // 获得了完整的HTTP请求 -> 构造响应报文
        BAD_REQUEST,    // HTTP请求报文有语法错误
        NO_RESOURCE,    // 请求资源不存在
        FORBIDDEN_REQUEST,  // 请求资源禁止访问
        FILE_REQUEST,   // 请求资源可以访问
        INTERNAL_ERROR, // 服务器内部错误，该结果在主状态机逻辑switch的default下，一般不会触发
        CLOSED_CONNECTION
    };

    // HTTP的方法，实际只支持GET 和 POST
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
public:

    // 根据连接套接字，构造HTTP请求对象
    void init(int sockfd, const sockaddr_in &addr);
    // 关闭该HTTP连接
    void close_conn(bool real_close = true);

    //读取浏览器端发来的全部数据
    bool read_once();

    // 处理客户端请求报文（由工作线程调用）
    void process();

    //响应报文写入函数
    bool write();

    // 获得该连接的客户端地址信息
    sockaddr_in *get_address()
    {
        return &m_address;
    }

    // 同步线程初始化数据库读取表
    void initmysql_result(connection_pool *connPool);

private:
   
    void init();

    //从m_read_buf读取，并处理请求报文
    HTTP_CODE process_read();

    //从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATUS parse_line();

    // 获得m_read_buf的处理的行的起始位置
    char *get_line() { 
        return m_read_buf + m_start_line; 
    };

    //主状态机解析报文中的请求行数据
    HTTP_CODE parse_request_line(char *text);
    // 主状态机解析报文中的消息头的每一行
    HTTP_CODE parse_headers(char *text);
    // 主状态机解析报文中的消息体
    HTTP_CODE parse_content(char *text);

    // HTTP请求报文解析完毕后，对请求的文件进行分析
    HTTP_CODE do_request();


    // 根据请求文件的分析, 将HTTP响应报文写入套接字
    bool process_write(HTTP_CODE ret);
    // 以下都是帮助构造响应报文的函数
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

    void unmap();   // 解除之前使用mmap()对文件的映射操作

public:

    static int m_epollfd;       // 记录监听HTTP请求套接字的epoll结构
    static int m_user_count;    // 记录服务器中已经有多个HTTP请求
    MYSQL *mysql;              // 记录该HTTP请求分配的数据库连接

private:
    int m_sockfd;   // 这个http请求在服务端分配的连接套接字的文件描述符
    sockaddr_in m_address;  // 这个http请求的客户端的地址情况

    char m_read_buf[READ_BUFFER_SIZE];  // 存储读取的请求报文数据(长度一定够一个完整的请求报文)
    int m_read_idx;     // 缓冲区中m_read_buf中数据的最后一个字节的下一个位置（实际上是记录读取的报文的长度）

    
    char m_write_buf[WRITE_BUFFER_SIZE];    // 存储相应报文的内容
    int m_write_idx;    // m_write_buf中数据的最后一个字节的下一个位置（实际上是记录读取的报文的长度）


    CHECK_STATE m_check_state;  // 主状态机的状态

    int m_checked_idx;  // 状态机中，记录m_read_buf读取的位置
    int m_start_line;   // 状态机中，记录m_read_buf中处理的行的起始位置

    
    //以下为解析请求报文中对应的变量
    METHOD m_method;        // HTTP请求的方法字段
    int cgi;                    //是否启用的POST
    char *m_version;      // HTTP请求的版本字段
    char *m_url;        // HTTP请求的URL
    char *m_host;       // HTTP报文中的Host信息
    int m_content_length;   // 如果是post请求，则有消息体，该字段记录消息体大小
    bool m_linger;      // 判断是否开启长连接   
    char *m_string;     //存储HTTP请求中消息头数据

    char m_real_file[FILENAME_LEN];     // 请求报文要获得的文件的完整路径
    struct stat m_file_stat;    // 请求报文要获得的文件的状态
    char *m_file_address;   // 将请求要获得的文件映射到内存中的地址

    struct iovec m_iv[2];   // writev进行集中写的数组
    int m_iv_count;     // 记录数组使用的大小

    //发送的报文的信息
    int bytes_to_send;      // 记录需要发送的字节
    int bytes_have_send;    // 已发送字节数
};


#endif