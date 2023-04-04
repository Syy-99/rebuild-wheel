## 线程同步机制封装类——`locker.h`

利用RAII思想，管理信号量，互斥量，条件变量

---

<font color='red'>Q：线程同步的机制有哪些?提供了哪些API?</font>

- 线条同步机制：信号量，条件变量，互斥量

- 相关API，参考《Linux高性能服务器编程》


<font color='red'>Q：是否可以考虑直接使用智能指针来避免增加这个类呢</font>

- 智能指针管理的是内存区域，析构时调用的是`delete`, 而我们这里销毁信号量/条件变量/互斥量都是使用相关API，因此不能直接使用智能指针，需要自定义管理类和相关构造、析构函数


<font color='red'>Q: 如何使用条件变量?</font>

- 条件变量的条件是在外部判断的
- 一旦外部代码判断条件不满足，则调用`pthread_cond_wait()`将线程加入对应条件变量的请求队列中
  - 因为条件变量的请求队列也是一个共享的资源，因此调用`pthread_cond_wait()`前需要加锁
  - 一旦线程成功加入，就需要释放该锁，使得其他线程可以操作该队列，唤醒其中的线程

## 同步&异步日志系统

使用**单例模式**创建日志系统，对服务器运行状态、错误信息和访问数据进行记录

该系统可以实现按天分类，超记录分类功能，可以根据实际情况分别使用同步和异步写入两种方式。

- 同步日志：日志写入函数与工作线程串行执行
- **异步日志**：将生产者-消费者模型封装为**阻塞队列**，**创建一个写线程**，工作线程将要写的内容push进队列，**写线程从队列中取出内容，写入日志文件**

---
<font color='red'>Q: 单例模式的定义和实现方式?</font>

- 略

<font color='red'>Q: 阻塞队列如何实现生产者-消费者模型?</font>

- 见下面介绍

### 阻塞队列——`block_queue.h`

阻塞队列类中封装了生产者-消费者模型，其中push成员是生产者，pop成员是消费者。

- 两个操作都支持阻塞：
  - **队列满时，队列会阻塞插入元素的线程，直到队列不满**
  - 队列空时，获取元素的线程会阻塞，等待被唤醒

- 使用了**循环数组**模拟队列，作为两者共享缓冲区

---

<font color='red'>Q: 循环数组如何模拟实现队列的?</font>

- 数组中保存的是`string`类型的元素，实际上就是此次需要写入日志的信息

  <font color='red'>Q: string不是内存可变的吗?那么`new string`会分配多少内存?</font>

  - `string`变量是一种特殊的对象，它的内部保存了一个指针，指向一块内存， 这块内存用户存储字符串
  - 因此`sizeof(string变量)`只会计算该指针和其他成员的内存大小，而和它实际保存字符串的大小有关

- `block_queue`类中提供了数组的头指针和尾指针

  ```c++
  // block.queue.h
  // 模拟队列, 保存队头和队尾的位置索引
  int block_queue_front_;	// 指向队头的前一个位置（取出）
  int block_queue_back_;	// 指向队尾位置（插入）
  
  // 初始化
  block_queue_front_ = -1;
  block_queue_back_ = -1;
  ```

  <font color='red'>Q: 为什么要这么设计两个指针的含义?感觉这样设计不好，不容易理解?</font>

  - 可以这样设计：

    ```
    block_queue_front_ 队头元素;block_queue_back_队尾元素
    初始都为0
    ```

    但是这样需要修改下的push和pop操作 ==修改==

    https://www.51cto.com/article/656335.html

- 插入和弹出时都需要对大小取余

  ```c++
  block_queue_back_ = (block_queue_back_ + 1) % block_queue_max_size_; // 循环使用
  block_queue_ptr_[block_queue_back_] = item;
  
  block_queue_front_ = (block_queue_front_ + 1) % block_queue_max_size_;
  item = block_queue_ptr_[block_queue_front_]; // 注意这里顺序，front先加1
  ```

<font color='red'>Q: 是否可以考虑直接使用队列来实现?</font>

- ==修改==，可考虑使用STL中的`deque`直接实现

<font color='red'>Q: 代码逻辑问题：如果阻塞队列已经满了，那么该日志记录的插入操作没有完成，不就没有记录到日志文件中?</font>

考虑`block_queue.push()`，如果队列满，则唤醒其他读线程，然后返回false, 那该日志记录不就没有加入阻塞队列，最终也就不会写入日志?

```c++
// 如果阻塞队列已经满了，则无法插入，必须要其他线程读才行
if (block_queue_size_ >= block_queue_max_size_)
{
    block_queue_cond_.broadcast(); // 唤醒等待的线程(只有读线程会等待)
    block_queue_mutex_.unlock();
    return false;		// 返回false
}
```

- ==修改==，这部分代码好像确实有问题，但是好像不太好实现

- 我最开始想的是外部利用while循环，但是这样就阻塞程序运行

- <font color = 'red'>感觉可以开一个线程来专门处理，但是如果只是一个线程，那其他日志信息不也无法写入吗?难道要每次写日志都开一个线程?</font>

### 日志——`log.h`&`log.cpp`

- 使用静态局部变量的方式，实现线程安全的C++单例模式

  - C++11下，单例模式还需要禁止移动构造和赋值函数 ==修改==

    ```c++
    // 单例模式需要禁止移动构造函数
    // Log(Log &&) = delete;
    // Log &operator=(Log &&) = delete;
    ```

- 新建日志文件的逻辑：天数不同 or 该日志文件的记录超过限定

  ```c++
  log_line_count_++;  // 日志已经写入的记录数
  
  // 判断是否需要新建log file
  if (log_file_today != my_tm.tm_mday || log_line_count_ % log_max_line_ == 0){
  //...
  }
  ```

  <font color='red'>Q: 只按照天来判断是否创建新的日志文件是否不合理呢?</font>

  - 感觉应该按照年+月+日的方式吧?==修改==

  <font color='red'>Q: 为什们这里先改变日志记录?</font>

  - 注意，`log_line_count_`从0开始，所以实际上现在写入的数量就是`log_line_count_ + 1`
  
  - 接着判断时间和日志中已经写入的记录数，来决定是否需要创建新日志文件

- 提供四种宏，输出四种等级的日志信息：

  ```c++
  #define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__)
  //...
  ```

---

<font color='red'>Q: `log`的析构函数是否需要特别关闭打开的文件?</font>

```c++
Log::~Log()
{
    if (m_fp != NULL)
    {
        fclose(m_fp);
    }
}
```

- 当 C++ 程序终止时，它会**自动关闭刷新所有流，释放所有分配的内存，并关闭所有打开的文件**；

  因此，应该可以不用特别来关闭打开的文件

  > 但程序员应该养成一个好习惯，在程序终止前关闭所有打开的文件。

<font color='red'>Q: 为什么函数定义中没有提供默认参数?</font>

```c++

bool Log::init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size){
  //...
}

```

- C++ 规定，在给定的作用域（文件作用域）中只能指定一次默认参数。

[C++函数的默认参数补充 ](https://www.cnblogs.com/chenke1731/p/9651275.html)

<font color='red'>Q: 代码逻辑问题，`init()`中如果创建的文件不包含目录，似乎没有保存该文件名?但是后续可能需要该文件名构造新的日志</font>

```c++
if (p == NULL)
{
    snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    // strcpy(log_file_name_, file_name);
}
else
{
    strcpy(log_name, p + 1);
    strncpy(dir_name, file_name, p - file_name + 1);
    snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
}
```
- 这个应该有问题，在我的代码中添加了注释的代码

<font color='red'>Q: 代码逻辑问题，下面的代码，当`log_queue`满了，那这个日志记录不就丢失了?</font>

```c++
if (log_is_async_ && !log_queue_->full())   // 异步日志先写到内存中
{
    log_queue_->push(log_str);
}
```
- 这个问题，和上面将日志信息写入一个满的block_queue类似


<font color='red'>Q: 代码设计问题：异步日志的意义在哪里? </font>

考虑下面负载异步将日志刷新到磁盘的线程代码, 只要阻塞队列不为空，就会从其中取一个日志信息写入文件，那这样可能出现：写入一个日志到缓冲，然后就调用该线程将其写入磁盘。那这样，感觉异步就没有意义了
```c++
void async_write_log() // 异步线程，将日志写到文件中
{
    string single_log;
    // 从阻塞队列中取出一个日志string，写入文件
    while (log_queue_->pop(single_log)) // 正常来说，这个while不会结束
    {
        log_file_mutex.lock();
        fputs(single_log.c_str(), log_file_fd_);    // 写到文件中
        log_file_mutex.unlock();
    }
}
```
- 感觉这里的设计是有点问题，应该设置为当阻塞队列有一定的数量，再将日志一次性写入
  
  >异步日志：在记录日志时，不需要等待日志写入到磁盘后才返回程序的执行结果，而是在内存中先缓存日志，等到一定的时间或缓存量达到一定的阈值后，再一次性将缓存的日志批量写入到磁盘中
  >
  >同步日志是指在记录日志时，需要等待日志写入到磁盘后才会返回程序的执行结果
- ==修改==

> 面试题：
>
> 1. 异步日志不会出现日志乱序的问题吗?
>   - 首先，日志插入缓存是按其生成时间插入的
>   - 其次，日志线程是单线程，每次按顺序从缓存中去日志信息写入文件，因此不会出现乱序问题

## 数据库连接池——`sql_connection_pool.h`&`sql_connection_pool.cpp`

- 数据库连接池对象管理一组数据库连接，由程序动态地对池中的连接进行使用，释放。
- 连接池对象是单例模式
- 利用RAII机制释放每一个数据库连接

- C++连接MySQL的API使用：https://juejin.cn/post/7141190286838857735

---

<font color='red'>Q：使用`connectionRAII`类来管理数据库连接，那是否该类应该也是单例模式呢?否则可以通过复制该对象进行考虑其中的数据库连接，导致出现问题?</font>

<font color='red'>Q: 如果连接池中没有可用连接，那是否应该在新建一个连接呢?而不是阻塞呢</font>

考虑下面代码，如果数据库连接池中所有的连接都用完了，那这里就会一直阻塞，直到某个客户端断开连接，释放一个数据库连接
```c++
// 当有请求时，从数据库中返回一个数据库连接
MYSQL *connection_pool::GetConnection() 
{
    MYSQL *con = nullptr;

    if (0 == connect_list_.size()) {  //? 有问题吧? 感觉和下面逻辑冲突了
        return nullptr;     
    }

    reserve.wait();     // 更新信号量，如果为0，则阻塞
    //...
}
```

这里的逻辑应该不合理	

而且，这段代码好像也有问题

​    - 首先，如果size()==0就直接返回nullptr，那个信号量的wait就不可能阻塞了

​    - 即使有两个线程在size()=1时执行if,然后其中一个执行了wait后正常执行, 那么另一个就阻塞，直到有连接释放，这个逻辑是否不合理呢

​    - 个人认为，只有在size()不等于0的时候才调用连接池，否则就直接创建

<font color='red'>Q: 为什么这里析构函数需要特别断开数据库连接?</font>

- MysQl服务器的连接断开操作通常是由客户端通知才会进行，因此必须对每个连接调用`mysql_close()`函数

<font color='red'>Q: 为什们数据库连接池对象需要额外使用一个类来实现RAII，而log对象却不需要做同样的操作呢?</font>

  - `connectionRAII`这个类只是用来管理从数据库连接池中**获取的连接**，是**为了确保从中获取的连接一定会返回到连接池中**

## 半同步半反应堆线程池

### 事件处理模型——同步I/O模型实现Proactor模型

流程如下：

- 主线程往epoll内核事件表注册socket上的读就绪事件。

- 主线程调用epoll_wait等待socket上有数据可读

- 当socket上有数据可读，**epoll_wait通知主线程,主线程从socket循环读取数据，直到没有更多数据可读，然后将读取到的数据封装成一个请求对象并插入请求队列**。

- 睡眠在请求队列上某个工作线程被唤醒，它**获得请求对象并处理客户请求，然后往epoll内核事件表中注册该socket上的写就绪事件**

- 主线程调用epoll_wait等待socket可写。

- **当socket上有数据可写，epoll_wait通知主线程。主线程往socket上写入服务器处理客户请求的结果。**

<font color='red'>Q:是否可以尝试实现一下Reactor模型</font>

### 并发模型——半同步/半反应堆

设计特点如下：

- 主线程充当异步线程，负责监听所有socket上的事件
- 若有新请求到来，**主线程接收之**,以得到新的连接socket，然后往epoll内核事件表中注册该socket上的读写事件
- 如果连接socket上有读写事件发生，**主线程从socket上接收数据，并将数据封装成请求对象插入到请求队列中**
- 所有工作线程睡眠在请求队列上，当有任务到来时，通过竞争（如互斥锁）获得任务的接管权

<font color='red'>Q：是否可以尝试实现【高效半同步/半异步模式】?</font>

### one thread one loop 思想

one thread one loop，翻译成中文的意思就是一个线程对应一个循环

也就是说一个**每个线程函数里面有一个循环流程，这些循环流程里面做的都是相同的事情**

```c++
//线程函数
void thread_func(void* thread_arg)
{
	//这里做一些需要的初始化工作

	while (线程退出标志)
	{
		//步骤一：利用select/poll/epoll等IO复用技术，分离出读写事件

		//步骤二：处理读事件或写事件

		//步骤三：做一些其他的事情
	}


	//这里做一些需要的清理工具
}
```

- 在这个项目中，体现在`void threadpool<T>::run()`函数

---

<font color='red'>Q: 代码逻辑问题：如果请求队列满了，那是否意味着这个HTTP请求报文就丢失了，无法被处理了呢?</font>

考虑下面的代码：

```c++
template <typename T>
bool threadpool<T>::append(T *request)
{
    queue_lock_.lock();

    // 如果请求队列已经满了，则返回false
    if (req_queue_.size() > max_requests)
    {
        queue_lock_.unlock();
        return false;
    }
    //...
}
```

<font color='red'>Q：代码逻辑问题：</font>

```c++
template <typename T>
void threadpool<T>::run()
{    
    //...
	Queue_state_.wait();      // 信号量减1(改变请求队列状态)

    Queue_lock_.lock();     // 加锁访问

    // 如果请求队列中没有请求，则不需要处理
    // 同上面的信号量减1的操作，应该不会到这个逻辑呀?</font>
    if (req_Queue_.empty())
    {
        Queue_lock_.unlock();
        continue;
    }
    //....
}
```

## HTTP连接处理

- 包括处理HTTP请求报文和构造HTTP相应报文

### 状态机解析HTTP请求

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/6OkibcrXVmBH2ZO50WrURwTiaNKTH7tCia3AR4WeKu2EEzSgKibXzG4oa4WaPfGutwBqCJtemia3rc5V1wupvOLFjzQ/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)

---

<font color='red'>Q: 代码设计问题，为什么需要一次性读出来保存在内存中，而不是根据用户的输入进行查表操作呢而且这个`initmysql_result()`和类也没有太大关系，是否应该抽离出来形成一个普通函数呢</font>

```c++
//将表中的用户名和密码放入map
map<string, string> users;
void http_conn::initmysql_result(connection_pool *coonPool);
```

<font color='red'>Q: Socket套接字什么时候需要开启`EPOLLONESHOT`</font>

- `EPOLLONESHOT`：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里

- 通常，我们期望的是一个socket连接套接字在任一时刻都只被一个线程处理，因此对连接套接字会开启该选项

- **当线程处理完后，需要通过epoll_ctl重置epolloneshot事件**

<font color='red'>Q：构造响应报文时，如何根据URL跳转到相应的界面</font>

```C++
 if (*(p + 1) == '3')
 //..
```

- 实际上，这个和HTML有关，当你点击一个按钮后，基于`action`属性会自动加到HTTP请求的URL中去

    ```html
    <!--judge.html-->
    <form action="0" method="post">
    <div align="center"><button type="submit">新用户</button></div>
    </form>
    ```
- 根据这个`action`属性，来返回对应的页面

## `main.cpp`

### 统一事件源

Linux下的信号采用的异步处理机制，信号处理函数和当前进程是两条不同的执行路线。

具体的，当进程收到信号时，操作系统会中断进程当前的正常流程，转而进入信号处理函数执行操作，完成后再返回中断的地方继续执行。

为避免信号竞态现象发生，信号处理期间系统不会再次触发它，即屏蔽该信号

一般的信号处理函数需要处理该信号对应的逻辑，当该逻辑比较复杂时，信号处理函数执行时间过长，会导致信号屏蔽太久。

这里的解决方案是：**信号处理函数仅仅发送信号通知程序主循环，将信号对应的处理逻辑放在程序主循环中，由主循环执行信号对应的逻辑代码。**

- 信号处理函数往管道的写端写入信号值
- 主循环则从管道的读端读出信号值

> 主进程使用I/O复用系统调用来监听管道读端的可读事件,这样信号事件与其他文件描述符都可以通过epoll来监测，从而实现统一处理。

---

<font color='red'>Q：为什们整个项目中套接字（监听套接字和连接套接字）都需要设置为非阻塞的?</font>

```c++
// 将监听套接字注册到内核事件表中，并设置其为非阻塞
addfd(epollfd, listenfd, false);    // 监听套接字不需要设置EPOLLONESHOT
```

- 整个项目是使用`epoll`实现IO多路复用，其思想是: **系统不阻塞在某个具体的IO操作上，而是阻塞在`select`、`poll`、`epoll`这些IO复用上的**

- 因此，如果监听套接字是阻塞的，那么可能会阻塞在`accpet`上，与上述思想不符

  > 实际上，监听套接字阻塞or非阻塞不影响

- 而且，因为ET模式是可选的，所以连接套接字必须是非阻塞的

<font color='red'>Q: 利用管道实现统一信号源，为什么管道的两端都是非阻塞的呢?</font>

```c++
// 创建管道， 这里的管道是为了实现统一事件源
// 管道写端写入信号值，管道读端通过I/O复用系统监测读事件
int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
assert(ret != -1);
addfd(epollfd, pipefd[0], false);   // 将管道的读端注册到内核事件表中，非阻塞
setnonblocking(pipefd[1]);          // 设置管道写端非阻塞
```

- 管道写端有信号处理函数使用，将信号值写进管道，如果写缓冲满了，则会阻塞在信号处理函数，这和统一时间源的思想不符合

  <font color='red'>Q: 没有对非阻塞返回值处理，如果阻塞是不是意味着这一次定时事件失效了?</font>

  - 是的，但定时事件是非必须立即处理的事件，可以允许这样的情况发生。

- 管道的读端非阻塞不影响，因为会使用while函数了处理

<font color='red'>Q:服务器需要处理的信号有哪些?</font>

- `SIGPIPE`: 对一个对端已经关闭的socket调用两次write, 第一次收到会收到一个	，第二次将会生成**SIGPIPE**信号, 该信号默认结束进程.
- `SIGTERM`：终止程序信号
- `SIGALRM`：定时器信号，该项目中主要用来释放不活跃的连接

<font color='red'>Q：代码设计问题：这里为什们要设置每隔TIMESLOT就触发一次SIGALARM信号呢?</font>

```c++
alarm(TIMESLOT);
```

- alarm的信号处理函数通过统一事件源传递给主循环，然后会去遍历定时器容器中的定时器是否超时，来释放连接

- 所以实际上这是一个周期检查的工作

<font color='red'>Q: `EINTR`是什么错误</font>
```c++
int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
if (number < 0 && errno != EINTR){
        break;
}
```
- 在epoll_wait时，因为设置了alarm定时触发警告，导致返回-1，errno为EINTR

- 对于这种错误返回, 忽略这种错误

## 非活动连接定时器

 <font color='red'>Q: 是否可以尝试使用【时间轮】实现定时器容器类，应该会更高效?</font>

 <font color='red'>Q：代码设计问题，是否可以使用【虚拟头结点】的思想，统一处理?（不需要对头结点特殊处理）</font>

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
