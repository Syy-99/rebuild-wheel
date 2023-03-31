
- 头文件不要用`using`指示, 用`using`声明，或直接使用`std`声明作用域

## `main.c`

Q: `basename()`函数有什么作用？

- 返回不含路径的文件字符串

- 需要添加头文件：<libgen.h>


## 同步/异步日志系统 ——log目录

同步/异步日志系统主要涉及了两个模块，一个是日志模块，一个是阻塞队列模块,其中加入阻塞队列模块主要是解决异步写入日志做准备.

- **单例模式**创建日志对象

- 同步日志：默认使用方式

- 异步日志： 自定义一个阻塞队列，实现异步日志的读写

- 实现按天、超记录(一个日志文件只能保存一定日志条数）的分类，超过了限制就创建新的日志文件

### `log.h` & `log.cpp`

Q: 只按照天来判断是否创建新的日志文件是否不合理呢？

```c++
// 判断是否需要新建log file
if (log_file_today != my_tm.tm_mday || log_line_count_ % log_max_line_ == 0) {
    //...
}
```

Q: 为什么函数定义中没有提供默认参数？

```c++
bool Log::init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size){
    //...
}

- C++ 规定，在给定的作用域（文件作用域）中只能指定一次默认参数。

[C++函数的默认参数补充 ](https://www.cnblogs.com/chenke1731/p/9651275.html)

Q：为什们获得当前时间要提供两种方式？

```c++
// 方式1：
time_t t = time(NULL);
// 方式2：
struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
```

- 目前看来，使用【方式2】可以获得当前时间的毫秒，如果不需要输出这个时间，也可以使用方式1

### `block_queue.h` & `block_quque.cpp`

- 阻塞队列是一个string类型的模拟队列，实际上，每个string元素都是一个日志记录


Q: `size() `如果返回后，切换到其他线程，向队列中添加了元素，那获得的长度不就不对了吗？
```c++
// 返回目前队列的大小
int size() 
{
    int tmp = 0;

    m_mutex.lock();
    tmp = m_size;

    m_mutex.unlock();
    return tmp;
}
```

Q：为什么在CLion中分两个文件，编译时提示`log.cpp`中缺少`block_queue`的引用吗？但是如果将所有成员函数实现写在`.h`中却可以成功编译
```
CMakeFiles\untitled.dir/objects.a(log.cpp.obj): In function `Log::init(char const*, int, int, int)':
C:/Users/Admin/CLionProjects/untitled/log/log.cpp:29: undefined reference to `block_queue<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > >::block_queue(int)'
CMakeFiles\untitled.dir/objects.a(log.cpp.obj): In function `Log::write_log(int, char const*, ...)':
```

```Makefile
cmake_minimum_required(VERSION 3.20)
project(untitled)

set(CMAKE_CXX_STANDARD 14)

add_executable(untitled log/block_queue.cpp log/block_queue.h log/log.cpp log/log.h main.cpp)
```



## 线程同步机制 —— lock目录

- 线程同步的方法：信号量；互斥量；条件变量

- 利用RAII机制管理资源

### `locker.h`

Q：是否可以考虑直接使用智能指针来避免增加这个类呢?

Q: 如何使用条件变量？

    - 条件变量的值实际上就类似一个01的信号量

    - 线程是否满足条件，有其他代码决定，而不是由条件变量决定


## MySQL数据库

- 数据库访问的流程：创建数据库连接 -> 进行数据库操作 -> 断开数据库连接

- 连接 mysql 的服务器 ，需要提供如下连接数据：

    - 服务器的 IP；

    - 服务器监听的端口 （默认 3306）；

    - 连接服务器的用户名 （默认 root） ，和 用户对应的 密码；

    - 需要操作的具体数据库名；

- C++连接MySQL的API使用：https://juejin.cn/post/7141190286838857735

### `sql_connect_pool`

<font color='red'>Q: 建立MySQL连接时，不是需要输入用户名和用户密码吗? 那连接池中的连接如何设置这两个参数呢？如果提前设置的话，那其他用户不就使用了其他人的账号登录数据库了吗？</font>
```c++
//main.cpp
connPool->init("localhost", "syy", "", "yourdb", 3306, 8);  // 初始化数据库连接池（8个连接）
```

- 代码中，数据库连接池输入的密码和账户是访问MySQL的账号和密码，并不是用来验证用户的账号和密码的

- 我们需要进入数据库，从数据库中的`user`表中获得该数据库支持的所有用户账户信息

    ```c++
    //http_conn.cpp
    void http_conn::initmysql_result(connection_pool *coonPool);   // 获得数据库中user表中的所有记录
    ```

- 用户在浏览器中进行登录操作，我们根据用户输入的信息在user表中验证


<font color='red'>Q: 为什们数据库连接池对象需要额外使用一个类来实现RAII，而log对象却不需要做同样的操作呢？</font>

    - 这里是我理解的问题，实际上`connectionRAII`这个类只是用来管理从数据库连接池中获取的连接的对象，是为了确保从中获取的连接一定会返回到连接池中

    - 如果只是考虑数据库连接池对象，是不需要这个额外类的（因为池对象在程序运行期间始终存在)

- 如果连接池中没有可用连接，那是否应该在新建一个连接呢？而不是阻塞呢?

    ```c++
    MYSQL *connection_pool::GetConnection()
    {   
        if (0 == connect_list_.size()) { 
            return nullptr;     
        }
        reserve.wait();     // 更新信号量，如果为0，则阻塞
        //...
    }
```
    - 这两段代码有冲突吧？

        - 首先，如果size()==0就直接返回nullptr，那个信号量的wait就不可能阻塞了

        - 即使有两个线程在size()=1时执行if,然后其中一个执行了wait, 那么另一个就阻塞，直到有连接释放，这个逻辑是否不合理呢（因为连接的释放取决于其他用户,不知道要阻塞多久)

        - 个人认为，只有在size()不等于0的时候才调用连接池，否则就直接创建，那么这里只需要一个互斥锁即可


## 线程池——`threadpool.h` & `threadpool.cpp`

Q: 代码逻辑问题?
```c++
    // 如果请求队列已经满了，则返回false
    if (req_queue_.size() > max_requests)
    {
        queue_lock_.unlock();
        return false;
    }
```
- 如果请求队列满了，是否会导致该请求被忽略呢？还是说在外部代码中有处理？
 
Q: 代码逻辑问题
```c++
template <typename T>
void threadpool<T>::run() 
{
    queue_state_.wait();      // 信号量减1
    queue_lock_.lock();
}
```
- 为什们用了信号量还需要用锁呢？

- 锁的作用是为了避免多个线程同时对线程池进行操作，这个每问题

- 如果这里不用信号量，就需要用if条件判断，但是我们知道if判断不是原子的，所有我个人的理解是利用信号量完成了if的判断

Q: 代码逻辑问题

```c++
    queue_state_.wait();      // 信号量减1
    queue_lock_.lock();
    // 如果请求队列中没有请求，则不需要处理
    // 同上面的信号量减1的操作，应该不会到这个逻辑呀？
    if (req_queue_.empty())
    {
        m_queuelocker.unlock();
        continue;
    }
```

## HTTP连接管理——`http_conn.h` & `http.conn.cpp`

Q: 代码设计问题，为什么需要一次性读出来保存在内存中，而不是根据用户的输入进行查表操作呢? 而且这个`initmysql_result()`和类也没有太大关系，是否应该抽离出来形成一个普通函数呢?
```c++
//将表中的用户名和密码放入map
map<string, string> users;
void http_conn::initmysql_result(connection_pool *coonPool);
```

Q: 代码设计问题，为什么服务器端连接套接字需要开启`EPOLLONESHOT`?这个选项到底有什么作用，如何起作用?
```c++
//初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true); // 将连接套接字加入epoll监听
    //...
}

Q: 代码设计问题， 第一个条件是干什么用的？
```c++
// http_conn::HTTP_CODE http_conn::process_read()
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        //...
    }
```
- 在GET请求报文中，每一行都是\r\n作为结束，所以对报文进行拆解时，仅用从状态机的状态line_status=parse_line())==LINE_OK语句即可

- 在POST请求报文中，消息体的末尾没有任何字符，所以不能使用从状态机的状态

    这里转而使用主状态机的状态作为循环入口条件

- 在完成消息体解析后，将line_status变量更改为LINE_OPEN，此时可以跳出循环，完成报文解析任务。

    Q: 怎么知道一个HTTP报文的消息体读完了(如果一个HTTP报文分包传输)？或者说，如果有两个报文到达，通过缓冲区读的时候怎么知道现在开始读第二个报文呢

        - 利用`content-length`字段

Q: 代码逻辑问题，如何理解主状态机处理消息体的流程？
```c++
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    //...
    // 判断是空行还是请求头
    if (text[0] == '\0')    // 如果是空行
    {
        //判断是GET还是POST请求
        if (m_content_length != 0)  // 如果m_content_length !=0,说明之前有Content-length字段，则主状态机转移
        {
            // 主状态机转移到消息体处理信息
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;      // ?? 为什们这里返回NO_REQUEST
        }
        return GET_REQUEST;    // 如果m_content_length ==0， 说明是GET请求
    }
    //...
}
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    //判断buffer中是否完整的读取了消息体
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
```

- 首先，我们假设这个HTTP报文有消息体，那么在`parse_headers`中，最终会改变主状态机`CHECK_STATE_CONTENT`，并且返回`NO_REQUEST`

- 主状态机中的下一次while循环会直接根据第一个条件进入，然后执行`CHECK_STATE_CONTENT`状态的处理逻辑, 即调用`parse_content`

- 注意到，此时`text`指向消息体的第一个位置, 又因为while是第一个条件进入的，此时m_checked_idx并没有改变，也就是说text也可以等于m_read_buf + m_checked_idx;

- 因此m_content_length + m_checked_idx等于该报文完整的长度（请求行+消息头+消息体)

- 因此，这里的if成立必须是读取到一个完整的HTTP报文才行，否则返回【请求不完整】

    此时，下次的循环就会从`parse_line()`中进行，去读取一行

Q：构造响应报文时，如何根据URL跳转到相应的界面?

```C++
 if (*(p + 1) == '3')
 //..
```

- 实际上，这个和HTLM有关，当你点击一个按钮后，基于`action`属性会自动加到HTTP请求的URL中去

    ```html
    <!--judge.html-->
    <form action="0" method="post">
        <div align="center"><button type="submit">新用户</button></div>
            </form>
    ```

## `main.cpp`


Q: 代码逻辑问题，注册的逻辑是什么？管道的两端分别给谁用？
```c++
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    addfd(epollfd, pipefd[0], false);   // 将管道[0]的读事件注册到内核事件表中？？？
    setnonblocking(pipefd[1]);          // 设置管道[1]非阻塞
```

Q: 为什么管道写端要非阻塞？

- send是将信息发送给套接字缓冲区，如果缓冲区满了，则会阻塞，这时候会进一步增加信号处理函数的执行时间，为此，将其修改为非阻塞。

Q: 没有对非阻塞返回值处理，如果阻塞是不是意味着这一次定时事件失效了？

    - 是的，但定时事件是非必须立即处理的事件，可以允许这样的情况发生。

Q: `EINTR`是什么错误?
```c++
int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
if (number < 0 && errno != EINTR){
        break;
}
```
- 在epoll_wait时，因为设置了alarm定时触发警告，导致返回-1，errno为EINTR

- 对于这种错误返回, 忽略这种错误

## 定时器——`lst_timer.h`

Q: 代码逻辑，为什们定时器类还需要连续资源？

- 首先，我们是对每一个客户连接都建立一个定时器

 - 因此；定时器必须知道它对应的连接，才能在超时时对关闭连接

 Q: 是否可以使用其他方式实现定时器容器类？

 - 可以通过【时间轮】实现，更高效，提高插入效率

 Q：代码逻辑，是否可以使用【虚拟头结点】的思想，统一处理？（不需要对头结点特殊处理）
```c++
void adjust_timer(util_timer *timer)
{
    //...
    //被调整定时器是链表头结点，将定时器取出，重新插入
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    } else{
        //...
    }
    //...
}
```
