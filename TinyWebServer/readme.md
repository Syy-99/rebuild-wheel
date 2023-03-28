
- 头文件不要用`using`指示, 用`using`声明，或直接使用`std`声明作用域

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

    - 

