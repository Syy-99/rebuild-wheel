
## 代码重构与理解
1. 按Google C++规范，修改程序的结构

    > 尽管是一个C程序，但是还是希望尽量保持良好的编程习惯
2. 添加代码，线程执行完成，回收线程所有的资源
    ```c
    //my_httpd - main()
    pthread_detach(new_thread);
    ```
3. 为什么要提供一个`get_line()`函数来读取HTTP请求报文?

    - 首先，C语言并没有读取一行的函数
    - 其次，之所以要一行一行的读取，是因为当执行cgi程序是，需要获得消息头中的Contedt-type的信息（通常是在第二行)，这样方便处理
4. Http请求报文相关知识：
    - 当访问http://localhost:4000/index.html时，HTTP请求报文头部的URL只会记录/index.html
    - 请求报文内字段间的空格可以有一个或多个区分，但最后一定是\r\n
    - 请求报文内消息头每个字段一行，以\r\n结尾
5. 编译warnning解决：
    ```c
    // int execl (const char *__path, const char *__arg, ...)
    execl(path, NULL); -> execl(path,  "",NULL);
    ```
