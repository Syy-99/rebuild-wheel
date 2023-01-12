## 概述
- 基于《程序员自我修养》第13章构造一个小型简单的CRT/CPPRT

- 具体逻辑见代码注释

- 该仓库內的CRT只支持在32位Linux系统上使用, 并且给CRT只是简化版本只实现了部分，但大致流程基本有

	> - 64位系统主要需要修改系统调用部分的汇编指令
	> - Windowns系统了解不多所以不写
	> - C++并没有完全实现，只是提供了和全局构造/析构相关的代码文件: `atexit.c/crtbeing.cpp/crtend.cpp/ctors.cpp`
## 源码分析

### 源码阅读

- 建议阅读顺序：根据`entry.c`中函数出现的顺序，到定义的文件中阅读

### 源码拓展

```c++
#ifdef __cplusplus
//...
```

- 实际上，gcc编译器在编译时会提供一些预定宏，我们才可以利用这些宏来进行判断
	和本程序相关的有：

	```sh
	__cplusplus：当编写C++程序时该标识符被定义
	WIN32
	linux
	```sh

## 编译实现

- 32位Linux系统

	```sh
	# 静态库编译+打包
	gcc -c -fno-builtin -nostdlib -fno-stack-protector entry.c malloc.c stdio.c string.c printf.c 
	ar -rs minicrt.a malloc.o printf.o stdio.o string.o

	# 程序编译+连接
	gcc -c -ggdb -fno-builtin -nostdlib -fno-stack-protector test.c 
	ld -static -e mini_crt_entry entry.o test.o minicrt.a -o test
	```

- 64位Linux系统: 需用添加`-m32`或`m elf_i386`选项 (未测试）

	```sh
	gcc -c -fno-builtin -nostdlib -fno-stack-protector entry.c malloc.c stdio.c string.c printf.c -m32 -g
	gcc -c -ggdb -fno-builtin -nostdlib -fno-stack-protector test.c -m32 -g
	ar -rs minicrt.a malloc.o printf.o stdio.o string.o
	ld -static -e mini_crt_entry entry.o test.o minicrt.a -o test -m elf_i386
	```

### 结果

![](res_my/result.png)
- 注意到,尽管我们输出有'\n',但是好像并没有被解析成换行

## 参考
- https://github.com/youzhonghui/MiniCRT

- https://github.com/MRNIU/MiniCRT

- https://blog.csdn.net/roger_ranger/article/details/78447366
