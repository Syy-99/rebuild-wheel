/*
http连接处理类

- 浏览器端发出http连接请求，主线程创建http对象接收请求并将所有数据读入对应buffer，将该对象插入任务队列，工作线程从任务队列中取出一个任务进行处理

- 工作线程取出任务后，调用process_read函数，通过主、从状态机对请求报文进行解析。

- 解析完之后，跳转do_request函数生成响应报文，通过process_write写入buffer，返回给浏览器端
*/

#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H


class http_conn
{
public:

     static const int READ_BUFFER_SIZE = 2048;  // 设置读缓冲区m_read_buf大小

public:

    //读取浏览器端发来的全部数据
    bool read_once();
private:

    char m_read_buf[READ_BUFFER_SIZE];  // 存储读取的请求报文数据
    
    int m_checked_idx;  // m_read_buf读取的位置
};


#endif