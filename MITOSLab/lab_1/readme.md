# Unix utilities

## Sleep

利用sleep系统调用，根据用户输入数字设置暂停的时间中断次数，并且如果用户没有设置则打印错误信息

### 实验步骤

1. 实现`user/sleep.c`：

2. 了解sleep系统调用流程：

    - `usr/Usys.s`中提供了`sleep()`的汇编形式，它会保存一个系统调用号，并将CPU切换到特权模式

    - `kernel/syscall.c`中的`syscall()`根据系统调用号，调用相应的函数`sys_sleep()`， 注意此时已经在内核态了
    
        > 更多系统调用相关分析，参考Lab_2

3. 在Makefile文件内添加sleep目标文件

    ```sh
    UPROGS=\
	#...
	$U/_zombie\
	$U/_sleep\   # 添加这一行，一定是_sleep,而不能是sleep！
    ```

### 结果

```sh
$ sleep 10
$ sleep
no specified number of tricks
$ sleep 2 2
too many command argument
$ sleep 100 
```
- 编译时，显示`user/user.h`中`uint`出错，需要`typedef`一下

```sh
syy@syyhost:~/xv6-labs-2020$ ./grade-lab-util sleep
make: 'kernel/kernel' is up to date.
== Test sleep, no arguments == sleep, no arguments: OK (1.4s) 
== Test sleep, returns == sleep, returns: OK (0.7s) 
== Test sleep, makes syscall == sleep, makes syscall: OK (1.1s) 
```

## pingpong 

父子进程利用管道通信：

- 父进程首先发送1字节后，子进程接收，并打印PID信息, 然后发送1字节

- 父进程接收后，打印PID信息

## 实验步骤

1. 实现`user/pingpong.c`:
    ```c
    #include "user/user.h"

    int main() {
        int fd[2];
        pipe(fd);

        if (fork() > 0) { // 在父进程中
            write(fd[1], "!", 1);
            
            char buf;
            read(fd[0], &buf, 1);
            printf("<%d>: received pong", getpid());	
        } else if (fork() == 0) {
            char buf;
            read(fd[0], &buf, 1);			// 因为父进程拥有fd[1], 因此当还没有数据时，read()会阻塞
            printf("<%d>: received ping", getpid());

            write(fd[1], "!", 1);
        } else {
            prinf("fork() error\n");
            return -1;
        }
    }
    ```
2. 在Makefile文件内添加pingpong目标文件

    ```
    $U/_sleep\
    $U/_pingpong\
    ```
### 结果

```
xv6 kernel is booting

hart 1 starting
hart 2 starting
init: starting sh
$ pingpong
5: received ping
3: received pong
$ QEMU: Terminated
syy@syyhost:~/xv6-labs-2020$ ./grade-lab-util pingpong
make: 'kernel/kernel' is up to date.
== Test pingpong == pingpong: OK (0.7s) 
```

- 为什们子进程的pid=5而不是3呢?

    ```sh
    # 消除pingpong.c的else部分
    $ pingpong
    fork() error 4    # ??? why?
    5: received ping
    3: received pong
    ```

## primes 

- 利用[筛法](https://swtch.com/~rsc/thread/)，获得[2,32]之间的所有素数

    1. main进程传递2,3..35

    2. 子进程基于传递的第一个数2，将所有是它整数倍的数筛掉后，然后将剩余数再传给下个子进程，重复处理步骤

## 实验步骤

1. 完成`user/priems`编写

2. 修改`Makefiel`文件

## 结果

```sh
$ primes
prime 2
prime 3
prime 5
prime 7
prime 11
prime 13
prime 17
prime 19
prime 23
prime 29
prime 31
$ QEMU: Terminated
syy@syyhost:~/xv6-labs-2020$ ./grade-lab-util primes
make: 'kernel/kernel' is up to date.
== Test primes == primes: OK (1.0s) 
```

## find

在指定目录下，根据文件名，寻找所有可能的路径

- 需要在目录内递归查找(除了. 和 ..)，即目录内的目录也需要查找

### 实验步骤

1. 阅读`user/ls.c`中的`ls()`，了解如何读取目录和文件

    - de.inum == 0是什么记录？
            
        ```sh
        if(de.inum == 0)        // 为什们innode number=0就直接跳过呢?
            continue;
        ```
        - 在xarg实验中，发现如果不加这个判断，会出错，但是不知道原因在哪

2. 参考`ls()`中目录的读取方法，完成`user/find.c`的编写

3. 修改`Makefile`文件

### 结果

```
$ echo > b
$ mkdir a
$ echo > a/b 
$ find . b
./b
./a/b
$ find /a b
/a/b
$ find a b
a/b
$ QEMU: Terminated
syy@syyhost:~/xv6-labs-2020$ ./grade-lab-util find
make: 'kernel/kernel' is up to date.
== Test find, in current directory == find, in current directory: OK (1.7s) 
== Test find, recursive == find, recursive: OK (1.5s) 
```

## xargs

写一个简化版本的[xargs程序](https://www.ruanyifeng.com/blog/2019/08/xargs-tutorial.html)：

- xargs命令的作用，是将标准输入转为命令行参数。

    ```sh
     $ echo hello too | xargs echo bye  # 等价于 echo bye hello too
    bye hello too       
    ```
- 因为是简化版本，即使从标准输入中获得多行，也只能一行一行处理


### 实验步骤

(同上，略)


### 结果

```sh
$ sh < xargstest.sh
$ $ $ $ $ $ hello
hello
hello
$ $ QEMU: Terminated
syy@syyhost:~/xv6-labs-2020$ ./grade-lab-util xargs
make: 'kernel/kernel' is up to date.
== Test xargs == xargs: OK (2.0s) 
```

### 拓展

- `exec(char *file, char *argv[])`中，参数也需要包括程序名

