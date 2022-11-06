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

## 编译实现

## 参考

- [Webbench源码分析](https://blog.csdn.net/yzhang6_10/article/details/51607239)

- [WebBench压力测试工具（详细源码注释+分析）](https://www.cnblogs.com/yinbiao/p/10784450.html)

- [源码剖析】Webbench —— 简洁而优美的压力测试工具](https://github.com/AngryHacker/articles/blob/master/src/code_reading/webbench.md#%E6%BA%90%E7%A0%81%E5%89%96%E6%9E%90webbench--%E7%AE%80%E6%B4%81%E8%80%8C%E4%BC%98%E7%BE%8E%E7%9A%84%E5%8E%8B%E5%8A%9B%E6%B5%8B%E8%AF%95%E5%B7%A5%E5%85%B7)

- [WebBench 源码分析 [不推荐阅读]](https://meik2333.com/posts/webbench-source-and-analysis/)