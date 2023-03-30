#include <map>
#include "http_conn.h"
using std::map;

//#define connfdET //边缘触发非阻塞
#define connfdLT //水平触发阻塞

//#define listenfdET //边缘触发非阻塞
#define listenfdLT //水平触发阻塞

//对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

#ifdef listenfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef listenfdLT
    event.events = EPOLLIN | EPOLLRDHUP;    // 监听套接字只需要读事件和挂起事件
#endif

    if (one_shot)
        event.events |= EPOLLONESHOT;
    
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);    // 设定监听套接字非阻塞读
}

// 从内核事件表中删除文件描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}


//循环读取客户数据，直到无数据可读或对方关闭连接
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE) // 要确保存储请求报文的缓冲区足够大才行
    {
        return false;
    }

    int bytes_read = 0;     // 记录此次读取的字节数量

#ifdef connfdLT

    bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0); // 将数据读到缓冲区中
    if (bytes_read <= 0)
    {
        return false;
    }
    
    m_read_idx += bytes_read;   

    return true;

#endif

//如果使用非阻塞ET工作模式，需要一次性将数据读完
#ifdef connfdET
    while (true)      // 使用while循环
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno ==     )
                break;
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
#endif
}

//将表中的用户名和密码放入map
map<string, string> users;
void http_conn::initmysql_result(connection_pool *coonPool)
{
     //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, coonPool);

    //在user表中检索username，passwd数据
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql)); //账户错误，记录到日志中
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

     //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);   // 第一列是账号
        string temp2(row[1]);   // 第二列是密码
        users[temp1] = temp2;
    }
}