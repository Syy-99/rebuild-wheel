# rebuild-wheel
该仓库主要记录了自己学习的一些项目

- 每个目录对应一个项目，主要包含以下内容：
    - 当时原始仓库的代码+个人注释+编译后的可执行文件

        > 所以如果你想具体理解代码每一步的意思，需要clone该文件到本地
    - 阅读后自己的代码实现, 以及个人认为原始代码不完善进行修改的地方
    - readme文档，主要是阅读代码的扩展知识（只针对自己当时的水平) 以及 自己代码实现时的一些问题或改进
- 每个目录下，原仓库文件要么名字保持不变，要么以_origin结尾; 自己创建的文件默认以my_结尾
- 如果在阅读的过程中发现错别字，及 bug ，请向本项目提交 **PR**

  如果觉得本仓库对你学习有帮助，请给仓库star!

## 目前完成的项目

> 注意：以下仓库都是我学习时fork下来的，不保证和最新版本的相同

1. [TinnyHttps](https://github.com/Syy-99/Tinyhttpd): 一个使用C语言开发简单的Web服务器，支持简单的cgi程序
    - 时间：
        ```
        2022.11.2 - 11.3  完成代码阅读
        2022.11.9 - 11.17 完成代码编写
        ```
        
        > 真心推荐: 看完了代码也需要自己敲一遍，即使按照原始思路编写，也会有更深的体会
    - 环境:
        ```sh
        syy@syyhost:~/WebBench$ cat /proc/version
        Linux version 5.15.0-52-generic (buildd@lcy02-amd64-032) (gcc (Ubuntu 11.2.0-19ubuntu1) 11.2.0, GNU ld (GNU Binutils for Ubuntu) 2.38) #58-Ubuntu SMP Thu Oct 13 08:03:55 UTC 2022
        ```
    - 学习建议：
        
        可以阅读一本简单网络编程书籍的书后再读，会觉得思路清晰

        我这里是在阅读完《TCP/IP网络编程》后进行该项目的学习
2. [WebBench](https://github.com/Syy-99/WebBench): 服务器压测工具
    - 时间：
        ```
        2022.11.5 - 11.7 完成代码阅读
        ```
    - 环境：和TinyHttpd相同
    - 学习建议：Linux网络编程 + HTTP相关知识
3. MiniCRT: Linux32位系统下的C运行时库
	- 时间：
		```
		2023.1.5 - 1.11 完成代码编写，成功运行测试程序
		```
	- 环境：32位系统
		```sh
		root@iZwz96ih3nchpyijnebdfiZ:~/MiniCRT# cat /proc/version
		Linux version 4.4.0-117-generic (buildd@lgw01-amd64-057) (gcc version 5.4.0 20160609 (Ubuntu 5.4.0-6ubuntu1~16.04.9) ) #141-Ubuntu SMP Tue Mar 13 12:01:47 UTC 2018
		root@iZwz96ih3nchpyijnebdfiZ:~/MiniCRT# uname -a
		Linux iZwz96ih3nchpyijnebdfiZ 4.4.0-117-generic #141-Ubuntu SMP Tue Mar 13 12:01:47 UTC 2018 i686 i686 i686 GNU/Linux
		```
## 正在进行的项目
- MIT操作系统实验

## 计划的项目
2. TinyWebServer(Web服务器，很多人用)
3. json-tutorial
4. MyTinySTL
5. sylar（服务器框架）
7. CSAPP实验

