# rebuild-wheel
该仓库主要记录了自己学习的一些项目
> 目前是为了求职QAQ —— 2022.11.1
- 每个目录对应一个项目，主要包含以下内容：
    - 当时原始仓库的代码+个人注释+编译后的可执行文件
        > 所以如果你想具体理解代码每一步的意思，需要clone该文件到本地
    - 阅读后自己的代码实现, 以及个人认为原始代码不完善进行修改的地方
    - readme文档，主要是阅读代码的扩展知识（只针对自己当时的水平) 以及 自己代码实现时的一些问题或改进
- 每个目录下，原仓库文件要么名字保持不变，要么以_origin结尾; 自己创建的文件默认以my_开头
- 如果在阅读的过程中发现错别字，及 bug ，请向本项目提交 **PR**

## 目前完成的项目
> 注意：以下仓库都是我学习时fork下来的，不保证和最新版本的相同

1. [TinnyHttps](https://github.com/Syy-99/Tinyhttpd): 一个使用C语言开发简单的Web服务器，支持简单的cgi程序
    - 时间：
        ```
        2022.11.2 - 11.3 完成代码阅读
        ```
    - 环境:
        ```sh
        syy@syyhost:~/WebBench$ cat /proc/version
        Linux version 5.15.0-52-generic (buildd@lcy02-amd64-032) (gcc (Ubuntu 11.2.0-19ubuntu1) 11.2.0, GNU ld (GNU Binutils for Ubuntu) 2.38) #58-Ubuntu SMP Thu Oct 13 08:03:55 UTC 2022
        ```
    - 学习建议：
        
        可以阅读一本简单网络编程书籍的书后再读，会觉得思路清晰

        我这里是在阅读完《TCP/IP网络编程》后进行该项目的学习

## 正在进行的项目
- Webbench
- MIT操作系统实验

## 计划的项目
1. WebBench(网站压测工具) 
2. TinyWebServer(Web服务器，很多人用)
3. json-tutorial
4. MyTinySTL
5. sylar（服务器框架）
6. MIT操作系统实验
7. CSAPP实验

