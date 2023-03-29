
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

<font color='red'>Q: 建立MySQL连接时，不是需要输入用户名和用户密码吗，那连接池中的连接如何设置这两个参数呢？如果提前设置的话，那其他用户不就使用了其他人的账号登录数据库了吗？</font>

<font color='red'>Q: 为什们数据库连接池对象需要额外使用一个类来实现RAII，而log对象却不需要做同样的操作呢？</font>


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