## 概述

- **[Tinyhttpd](https://github.com/EZLippi/Tinyhttpd)**: github仓库
- 一个超轻量型Http Server，使用C语言开发，全部代码不到600行，附带一个简单的Client，

## 源码分析

### 源码阅读

- 建议源码阅读顺序： main -> startup -> accept_request -> execute_cgi, 通晓主要工作流程后再仔细把每个函数的源码看一看。
- 各部分代码作用见程序注释

### 源码拓展

```c
// htpd.c-main()
u_short port = 4000;        // 服务器套接字监听的端口
```

- `u_short`：`<types.h>`中提供的类型别名

  ```
  u_short -> __u_short -> unsigned short int
  ```

- port最大为655532(2^16 - 1), short类型在32/64位都是2字节，使用无符号short刚好能表示所有port

---

```c
// htpd.c-main()
socklen_t  client_name_len = sizeof(client_name);
client_sock = accept(server_sock,(struct sockaddr *)&client_name, &client_name_len);
```

- `socklen_t`：`<socket.h>`中提供的类型别名

  ```
  socklen_t -> __socklen_t ->  __U32_TYPE -> unsigned int
  ```

- socket编程中的accept函数的第三个参数的长度必须和int的长度相同

  > Q: 那为什么不直接用int？
  >
  > POSIX开始的时候用的是size_t，但在32位机下，size_t和int的长度相同，都是32 bits,但在64位机下，size_t（32bits）和int（64 bits）的长度是不一样的
  >
  > 最终POSIX的那帮家伙找到了解决的办法,那就是创造了 一个新的类型"socklen_t".
  >
  > Linux Torvalds说这是由于他们发现了自己的错误但又不好意思向大家伙儿承认,所以另外创造了一个新的数据类型

---

```c
// htpd.c-main()
pthread_create(&newthread , NULL, (void *)accept_request, (void *)(intptr_t)client_sock)
```

- `intptr_t`:

  ```
  intptr_t -> __intptr_t -> __SWORD_TYPE -> long int
  ```

- intptr_t是为了跨平台，**其长度总是所在平台的位数**，所以用来存放地址。

---

```c
// htpd.c - unimplemented ()
sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n"); 
```

- HTTT请求和响应头中，换行符是\r\n。

- `\r、\n、\r\n`区别

  ```
  \r: 回车，会使得光标移到行首，会把该行之前的输出覆盖掉
  \n: 换行，使光标下移一格
  \r\n: 光标下移一格并且移动到行首
  ```

  - Unix系统里，每行结尾只有“**<换行>**”，即“`\n`”；

  - Windows系统里面，每行结尾是“**<换行><回车>**”，即“`\r\n`”；

    > - 通常用的`Enter`是两个加起来的，即`\r\n`
    > - **有的编辑器只认rn，有的编辑器则两个都认。所以要想通用的话，最好用rn换行。**

  - Mac系统里，每行结尾是“**<回车>**”,即`\r`。

---

```c
//  htpd.c - accept_request ()
if (stat(path, &st) == -1) 
    //...
    ;
//...

if ((st.st_mode & S_IFMT) == S_IFDIR)   // 如果该文件是个目录，则默认使用它下面的index.html文件
    strcat(path, "/index.html");
if ((st.st_mode & S_IXUSR) ||
    (st.st_mode & S_IXGRP) ||
    (st.st_mode & S_IXOTH)    )
```

- `stat()`: 通过文件名filename获取文件信息，并保存在buf所指的结构体stat中

  ```c
  #include <sys/stat.h>
  #include <unistd.h>
  int stat(const char *file_name, struct stat *buf);
  /*
  执行成功则返回0，失败返回-1，错误代码存于errno
  */
  ```

  - `stat`

    ```c
     struct stat {
        dev_t     st_dev;         /* 包含这个文件的设备 ID */
        ino_t     st_ino;         /* inode 编号 */
        mode_t    st_mode;        /* 访问权限 */
        nlink_t   st_nlink;       /* 硬链接数量 */
        uid_t     st_uid;         /* 用户ID */
        gid_t     st_gid;         /* 组ID */
        dev_t     st_rdev;        /* 设备ID */
        off_t     st_size;        /* 文件占用的字节数 */
        blksize_t st_blksize;     /* 文件系统块大小 */
        blkcnt_t  st_blocks;      /* 文件占用了几个512字节 */
        time_t    st_atime;       /* 最后访问时间 */
        time_t    st_mtime;       /* 最后更改时间 */
        time_t    st_ctime;       /* 最后状态更改时间 */
    };
    ```

    - | Constant | Test macro | File type        |
      | -------- | ---------- | ---------------- |
      | S_IFREG  | S_ISREG()  | Regular file     |
      | S_IFDIR  | S_ISDIR()  | Directory        |
      | S_IFCHR  | S_ISCHR()  | Character device |
      | S_IFBLK  | S_ISBLK()  | Block device     |
      | S_IFIFO  | S_ISFIFO() | FIFO or pipe     |
      | S_IFSOCK | S_ISSOCK() | Socket           |
      | S_IFLNK  | S_ISLNK()  | Symbolic link    |

      同时可以获取文件的权限信息

      | Constant | Octal value | Permission bit |
      | -------- | ----------- | -------------- |
      | S_ISUID  | 04000       | Set-user-ID    |
      | S_ISGID  | 02000       | Set-group-ID   |
      | S_ISVTX  | 01000       | Sticky         |
      | S_IRUSR  | 0400        | User-read      |
      | S_IWUSR  | 0200        | User-write     |
      | S_IXUSR  | 0100        | User-execute   |
      | S_IRGRP  | 040         | Group-read     |
      | S_IWGRP  | 020         | Group-write    |
      | S_IXGRP  | 010         | Group-execute  |
      | S_IROTH  | 04          | Other-read     |
      | S_IWOTH  | 02          | Other-write    |
      | S_IXOTH  | 01          | Other-execute  |

  - 错误代码：

    ```
    ENOENT         参数file_name指定的文件不存在
    ENOTDIR        路径中的目录存在但却非真正的目录
    ELOOP          欲打开的文件有过多符号连接问题，上限为16符号连接
    EFAULT         参数buf为无效指针，指向无法存在的内存空间
    EACCESS        存取文件时被拒绝
    ENOMEM         核心内存不足
    ENAMETOOLONG   参数file_name的路径名称太长
    ```

---

```c
// htpd.c - execute_cgi()
putenv(meth_env);
```

## 编译实现

这个项目是不能直接在Linux环境下编译运行的，它本来是在Solaris上实现的，需要修改几处地方

> - 修改的地方基于github仓库代码，并不是原官方代码
> - 个人版本：VM ware + Ubuntu 22.04.1 LTS

1. 安装perl
   - 系统平台上已经默认安装了 perl, 可以通过`perl -v`来验证

2. 安装CGI.pm

   ```sh
   1. perl -MCPAN -e shell
   2. 在cpan[1]>中输入:install CGI.pm
   ```

   - 安装需要一定的时间
   - 终端里执行`perl -MCGI -e 'print "CGI.pm version $CGI::VERSION\n";'`  这条命令来验证是否安装成功]

   [linux下perl及cgi.pm的安装(perl-5.22.1)](https://blog.csdn.net/Yihchu/article/details/50732944)

3. 修改`color.cgi`内指定的脚本执行路径为本机实际的路径

   ```
   #!/usr/local/bin/perl -Tw  --> #!/usr/bin/perl -Tw
   ```

4. 代码修改：

   - `accept_request()`

     ```
     1. 返回类型修改 void -> void*: 为了配合pthread_create
     2. accept_request()的函数声明同样修改
     3. accept_requeset()函数的内 retrun -> return NULL
     ```

### 结果

![image-20221103105905969](my_res\image-20221103105905969.png)

![image-20221103105923475](my_res\image-20221103105923475.png)

## 参考

- [TinyHTTPd--超轻量型Http Server源码分析](https://blog.csdn.net/wenqian1991/article/details/46011357)
- [TinyHTTPd 编译及 HTTP 浅析](https://blog.csdn.net/wenqian1991/article/details/46048987)

- [Tinyhttpd 源码解析](https://hanfeng.ink/post/tinyhttpd/)