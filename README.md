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
    
    - 学习目的：了解Socket编程的基本流程
    
    - 时间：
        ```
        2022.11.2 - 11.3  完成代码阅读
        2022.11.9 - 11.17 完成代码编写
        ```
    - 环境:
        ```sh
        syy@syyhost:~/WebBench$ cat /proc/version
        Linux version 5.15.0-52-generic (buildd@lcy02-amd64-032) (gcc (Ubuntu 11.2.0-19ubuntu1) 11.2.0, GNU ld (GNU Binutils for Ubuntu) 2.38) #58-Ubuntu SMP Thu Oct 13 08:03:55 UTC 2022
        syy@syyhost:~$ uname -a
        Linux syyhost 5.15.0-56-generic #62-Ubuntu SMP Tue Nov 22 19:54:14 UTC 2022 x86_64 x86_64 x86_64 GNU/Linux
        ```
    - 学习建议：
        
        可以阅读一本简单网络编程书籍的书后再读，会觉得思路清晰

        我这里是在阅读完《TCP/IP网络编程》后进行该项目的学习
2. [WebBench](https://github.com/Syy-99/WebBench): 服务器压测工具

    - 学习目的： TinyHttpd的附带项目学习 

    - 时间：
        ```
        2022.11.5 - 11.7 完成代码阅读
        2022.11.17 - 11.23 完成代码编写
        ```

    - 环境：和TinyHttpd相同
    - 学习建议：Linux网络编程 + HTTP相关知识

3. MiniCRT: Linux32位系统下的C运行时库

    - 学习目的： 完成《程序员的自我修养》书籍的阅读

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
	- 学习建议：
		阅读《程序员的自我修养》（强烈推荐）

4. MIT操作系统实验-2020: 经典操作操作系统实验，可以帮助我们理解操作系统相关实现和概念

    - 学习目的：加深对操作系统的理解

    - 时间：
        ```
        2023.2.7 - 3.20  除lab_10外的实验完成
        ```
        - 实际之前已经开始，但是觉得之前做的不够仔细，故重新开始
    - 环境：和TinyHttpd相同
    - 学习建议：
        1. 建议在每个实验前阅读相应章节的教材内容，视频可以不看
        2. 建议在进行实验室阅读相关源码，进行理解

5. TinyWebServer: C++面试常见项目，用的人很多，但是写的过程注意深挖知识点

    - 学习目录：了解高性能的Web服务器的框架，并发事件，线程池等技术在实际代码中的实现

    - 时间：
    
        ```
        代码理解+重写： 2023.3.20 - 4.2 

        readme整理：4.2 - 4. 4
        ```
    - 环境： 同上

    - 学习建议：

        - 建议阅读《Linux高性能服务器编程》

6. MyTinySTL: 实现一个轻量级的STL库

    - 学习目的：基于《STL源码分析》，加深对STL库的理解

    - 时间：

        ```
        ```

    - 环境：同上

    - 学习建议：
        
        - 可以参考《STL源码分析》——候捷，来理解
        
## 计划的项目

3. json-tutorial
4. MyTinySTL

