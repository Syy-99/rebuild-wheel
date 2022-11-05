## 概述

- **[WebBench](https://github.com/EZLippi/WebBench)**
- Linux下使用的非常简单的网站压测工具。它使用fork()模拟多个客户端同时访问我们设定的URL，测试网站在压力下工作的性能，最多可以模拟3万个并发连接去测试网站的负载能力

## 源码分析
### 源码阅读
- 程序工作流程：
![img](http://i.imgur.com/CEExOiJ.png)
- 项目组成：
    - 主要代码：socket.c & webbench.c



## 编译实现

## 参考
- [Webbench源码分析](https://blog.csdn.net/yzhang6_10/article/details/51607239)
- [WebBench 源码分析 [不推荐阅读]](https://meik2333.com/posts/webbench-source-and-analysis/)
- [WebBench压力测试工具（详细源码注释+分析）](https://www.cnblogs.com/yinbiao/p/10784450.html)（推荐）
- [源码剖析】Webbench —— 简洁而优美的压力测试工具](https://github.com/AngryHacker/articles/blob/master/src/code_reading/webbench.md#%E6%BA%90%E7%A0%81%E5%89%96%E6%9E%90webbench--%E7%AE%80%E6%B4%81%E8%80%8C%E4%BC%98%E7%BE%8E%E7%9A%84%E5%8E%8B%E5%8A%9B%E6%B5%8B%E8%AF%95%E5%B7%A5%E5%85%B7)
