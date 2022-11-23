## 代码重构和理解

1. 因为我并没有研究如何完整的在Linux中添加一个命令，所以这里我们只重写webench.c这个程序文件.g弄明白这里的实现逻辑即可
    - 修改HTTP方法，只使用GET
    - 修改HTTP版本，只实现1.1
    - 删除-p参数，不使用代理服务器;
    - 删除-r参数，不需要中间服务器缓存

> 虽然代码写完, 但是只能确保逻辑上正确，不确保最终能运行

2. 参考[WebBench 源码分析 [不推荐阅读]](https://meik2333.com/posts/webbench-source-and-analysis/),修改了一些逻辑的错误
2. 只在当前程序中实现的函数尽可能使用static限制
    ```c
    static void Help_info();
    //...
    ```
    - 实际上，按Google C++规范，应该放在命名空间内; 但是我们这里还是编写的C程序

3. 修改选项`-c`, 不要求一定有参数，默认参数是1
    ```c    
    // main()
    getopt_long(argc, argv,"frt:p:c01?hV",long_options,&long_index)

    // long_options[]
    {"clients", no_argument, NULL, 'c'},
    ```
4. 利用库函数`perror`直接将信息输入的stderr中
5. 构造请求报文时，需要考虑几种特殊的访问服务器的url形式
    ```
    http://192.158.1.1
    http://192.158.1.1/
    http://192.158.1.1:80
    http://192.158.1.1:80/
    http://192.158.1.1:80/index.html
    ```
6. 构造HTTP请求报文是，如何知道请求头部需要使用字段呢？
    - 除了一些必须的字段，其他字段根据需要设置
        
        > 目前，HTTP1.1只要求必须有Host字段


    [用浏览器访问网址时，请求头(request header)是根据什么生成的?](https://www.zhihu.com/question/34603729/answer/85911416)

    [浏览器如何确定http的请求首部字段？](https://www.zhihu.com/question/27165849/answer/160656587)
    
7. 可以不用子进程，用线程来完成压测？
    - 应该可以吧（个人没有测试）
8. 原生C语言没有`bool`类型
    ```c
    int is_time_expirted = 0;   // 尽管表示判断, 但是类型只能用int
    ```
9. 父进程如何等待所有子进程
    - `wait()`: 成功，返回被回收的子进程的id; 所有的子进程都结束后，证明函数调用失败，返回-1
    - `waitpid()`: 如果调用进程没有子进程,那么waitpid返回—1,并且设置errno为ECHILD

    [Linux中wait()和waitpid()函数](https://blog.csdn.net/weixin_45943332/article/details/111759366)

    [等待所有子进程结束](https://blog.csdn.net/yasi_xi/article/details/46341733)
11. 原始代码中，`speed` 和 `fail`变量到底声明意思?
    - 在每个子进程中,只有成功压测时，`speed++`, 因为`spped`记录的时成功的次数，这应该每问题；
      
      但是，`fail`在每次失败都会+1, 那么最终统计的时每个子进程中连接失败的次数 
    - 在最后父进程输出最终信息时
        ```c
        Speed=%d pages/min  -> (speed + failed) / (benchtime/60.0f);
        ```
        为什们这两个加起来除以分钟的单位是 `pages/min`呢? 这里pages是什么意思?
    

