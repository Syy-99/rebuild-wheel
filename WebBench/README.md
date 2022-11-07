## 概述

- **[WebBench](https://github.com/EZLippi/WebBench)**
- Linux下使用的非常简单的网站压测工具。它使用fork()模拟多个客户端同时访问我们设定的URL，测试网站在压力下工作的性能，最多可以模拟3万个并发连接去测试网站的负载能力

## 源码分析

### 源码阅读

- 程序工作流程：

  ![img](http://i.imgur.com/CEExOiJ.png)

- 项目组成：
    - 主要代码：socket.c & webbench.c

  

## 源码分析

```c
// webbench.c - main()
while((opt=getopt_long(argc,argv,"912Vfrt:p:c:?h",long_options,&options_index))!=EOF)
```

- `getopt_long()`为支持长选项的命令行参数函数，是Linux C库函数，需要包含`<getopt.h>`

  ```c
  int getopt_long(int argc, char * const argv[],const char *optstring,const struct option *longopts,
                  int *longindex);
  /*
  每次返回一个选项。如果遇到非法选项返回-1，其他情况根据参数返回具体值
  参数：
  argc: 命令行参数的个数(包括命令本身)
  argv：记录命令行参数的具体内容
  optstring：用于规定合法选项以及是否带参数
  longopts：用于规定合法长选项,可以是数组类型
  longindex: 表示长选项在longopts中的位置(该项主要用于调试, 一般设为 NULL 即可)
  */
  ```

  - 长选项：以`--`开头的选项

  - `argc` 和 `argv` 一般使用main()传递进来的值

  - `optstring`: 一般为合法选项字母构成的字符串，如果字母后面带上冒号`:`就说明该选项**必须**有参数

  - `option`:

    ```c
    struct option {
        const char *name;		// 长选项名称
        int has_arg;	// 参数情况（0:无参数；1:有参数；2: 参数可选）
        int *flag;	// 指定长选项如何返回结果.如果flag为NULL, getopt_long() 会返回val. 如果flag不为NULL, getopt_long会返回0, 并且将val的值存储到flag中
        int val;	// 将要被getopt_long返回或者存储到flag指向的变量中的值
    };
    ```

    - hat_arg一般使用符号常量而不是数值

      ```
      no_argument; required_argument; optional_argument;
      ```
    
    [命令行参数处理:getopt()和getopt_long()](https://www.jianshu.com/p/80cdbf718916)
    
    [【C】解析命令行参数--getopt和getopt_long](http://blog.zhangjikai.com/2016/03/05/%E3%80%90C%E3%80%91%E8%A7%A3%E6%9E%90%E5%91%BD%E4%BB%A4%E8%A1%8C%E5%8F%82%E6%95%B0--getopt%E5%92%8Cgetopt_long/)

---

```c
// webbench.c - main()
case 't': benchtime=atoi(optarg);break;	    
```

- `optarg`: 指向当前选项的参数
- benchtime: 压测时间，可以理解为客户端保持连接该服务器的时间

---

```c
// webbench.c - main()
tmp=strrchr(optarg,':');
```

- `char* strchr(const char *str, int c)`: 在参数str指向的字符串中搜索最后一次出现字符c的位置，未找到则返回空指针

---

```c
// webbench.c - main()
if (optind == argc)
```

- `optind`: 第一个不是命令选项的参数在argv中的索引

---

```c
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
```

- 支持的HTTP请求方法，但是现在通常只使用GET & POST

  [HTTP请求，关于HEAD请求](https://blog.csdn.net/weixin_45624150/article/details/108094729)

---

```c
//webbench.c - build_request()
if (0 != strncasecmp("http://", url, 7))
```

- `int strncasecmp(const char *s1, const char *s2, size_t n)`: 比较参数s1 和s2 字符串前n个字符，比较时会自动忽略大小写的差异
  - 若参数s1 和s2 字符串相同则返回0。s1 若大于s2 则返回大于0 的值，s1 若小于s2 则返回小于0 的值。

---

```c
//webbench.c - build_request()
// 从主机名开始的地方开始往后找，没有 '/' 则url非法
if (strchr(url + i, '/') == NULL)
{
    fprintf(stderr, "\nInvalid URL syntax - hostname don't ends with '/'.\n");
    exit(2);
}
```

- 这样设置是否合理？

  根据查找，个人认为这个设置是不合理的

  [在URL的末尾应该有一个斜杠吗？](https://ahrefs.com/blog/zh/trailing-slash/)

  [URL 的结尾需要斜线（/）吗？](https://www.zhihu.com/question/24573192)

---

```c
//webbench.c - build_request()
if (index(url + i, ':') != NULL && index(url + i, ':') < index(url + i, '/'))  
```

- `char *index(const char *s, int c);`
  - 找出参数s字符串中第一个出现的参数c地址，然后将该字符出现的地址返回。字符串结束字符(NULL)也视为字符串一部分。
  - 如果找到指定的字符则返回该字符所在地址，否则返回NULL

---

```c
//webbench.c - build_request()
strncpy(host, url + i, strcspn(url + i, "/"));
```

- `size_t strcspn(const char *str1, const char *str2)`: 检索字符串 str1 中第一个在字符串 str2 中出现的字符下标

---

```c
//webbench.c - build_request()
strcat(request + strlen(request), url + i + strcspn(url + i, "/"));
```

- 感觉这行代码有问题：
  - `char *strcat(char *dest,const char *src);`:把src所指字符串添加到dest结尾处(覆盖dest结尾处的'\0')并添加'\0'。
  - 那么完全没有必要使用request + strlen(request)吧

---

```c
// webbench.c - main()
printf(".\n");
```

- 为什们这里一定要加\n呢？

  - 因为fork会拷贝父进程的缓冲区

  - 标准I/O库提供了三种类型的缓冲：

    - 全缓冲：填满标准I/O缓冲区才实际进行I/O操作。

      > 磁盘文件通常是全缓冲

    - 行缓冲：在输入和输出中遇到换行符时，标准I/O库执行I/O操作，当流涉及终端时，通常使用行缓冲。

      > 涉及终端设备，如stdin stdout是行缓冲

    - 不带缓冲：标准出错流stderr通常是不带缓冲的。

  - 所以，如果不使用\n刷新，那么子进程的缓冲区会有这些数据，导致之后库函数读取缓冲出错

---

```c
// webbench.c - bench()
for (i = 0; i < clients; i++)
{
    pid = fork();
    if (pid <= (pid_t)0)
    {
        /* child process or error*/
        sleep(1); // 子进程挂起1毫秒，将cpu时间交给父进程
        break;  // 跳出循环，确保子进程不会创建子进程
    }
}

if (pid < (pid_t)0) // 如果子进程创建失败
{
    fprintf(stderr, "problems forking worker no. %d\n", i);
    perror("fork failed.");
    return 3;
}
```

- 这里有问题：
  1. 通过child中使用sleep(1)来设置父子进程优先级就不合理
  2. 循环外的if只能判断最后一个fork的子进程的情况

---

```c
// webbench.c - benchcore()
if (timerexpired)   // 如果到压测时间
{
    if (failed > 0)     // 并且这个子进程始终没有成功建立连接
    {
        /* fprintf(stderr,"Correcting failed by signal\n"); */
        failed--;   // 则恢复记录
    }
    return;
}
```

- 这里有问题：如果到压测时间并且始终没有连接成功，为什们要恢复记录?

---

```c
// webbench.c - bench()
setvbuf(f, NULL, _IONBF, 0);
```

- `int setvbuf(FILE *stream, char *buffer, int mode, size_t size)`： 定义流stream如何缓冲

  ```
  stream -- 这是指向 FILE 对象的指针，该 FILE 对象标识了一个打开的流。
  buffer -- 这是指向给用户分配的缓冲的地址。如果设置为 NULL，该函数会自动分配一个指定大小的缓冲。
  mode -- 这指定了文件缓冲的模式: _IOFBF 全缓冲; _IOLBF 行缓冲; IONBF 无缓冲;
  size -- 这是缓冲的大小，以字节为单位。
  ```

  

## 编译实现

## 参考

- [WebBench压力测试工具（详细源码注释+分析）](https://www.cnblogs.com/yinbiao/p/10784450.html)
- [源码剖析Webbench —— 简洁而优美的压力测试工具](https://github.com/AngryHacker/articles/blob/master/src/code_reading/webbench.md#%E6%BA%90%E7%A0%81%E5%89%96%E6%9E%90webbench--%E7%AE%80%E6%B4%81%E8%80%8C%E4%BC%98%E7%BE%8E%E7%9A%84%E5%8E%8B%E5%8A%9B%E6%B5%8B%E8%AF%95%E5%B7%A5%E5%85%B7)
- [WebBench 源码分析 [不推荐阅读]](https://meik2333.com/posts/webbench-source-and-analysis/)