# Lab: Multithreading

## Uthread: switching between threads

- design the context switch mechanism for a **user-level threading system**, and then implement it

- Your job is to come up with a plan to create threads and save/restore registers to switch between threads, and implement that plan

    > 实现在用户空间创建线程和执行线程切换

### 实验步骤

1. 查看`uthread.c`和`uthread_switch.S`中提供的用户层面的线程接口

2. 完善用户线程的上下文切换`uthread_switch.S`

    - 参考内核进程的`swtch()`的上下文切换过程

    - 见具体代码文件


完善`uthread.c`中的函数 

    - `thread_schedule()` 参考内核函数`scheduler)`,调用上下文切换函数即可 
    
    - `thread_create(void (*func)())`: 传递一个函数指针，指明该线程中运行的程序

        - 参考`allocproc()`函数，对用户线程的结构体进行初始化

> 实际上，这个就是用户线程模型，一个内核线程对应一个用户进程，用户进程中有多个用户线程
> 多个用户线程映射到一个内核线程，用户线程由用户进程自己管理
### 实验结果

```sh
thread_c: exit after 100
thread_a: exit after 100
thread_b: exit after 100
thread_schedule: no runnable threads
$ QEMU: Terminated
syy@syyhost:~/xv6-labs-2020$ ./grade-lab-thread Uthread
make: 'kernel/kernel' is up to date.
== Test uthread == uthread: OK (1.3s) 

```

- thread_switch needs to save/restore only the callee-save registers. Why?

    - thread_switch对于调用它的函数来说，也是一个callee, 只需要保存相应的寄存器即可

    - 对于其它 caller-save 寄存器都会被保存在线程的堆栈上, 在切换后的线程上下文恢复时可以直接从切换后线程的堆栈上恢复 caller-save 寄存器的值

        > 为什们没有从代码上看到这一步呢？

## Using threads

- you will explore parallel programming with threads and locks using a hash table

- You should do this assignment on a real Linux or MacOS computer (not xv6, not qemu) that has multiple cores.

    ```sh
    syy@syyhost:~/xv6-labs-2020$ cat /proc/cpuinfo| grep "physical id"| sort| uniq| wc -l
    2
    ```
- This assignment uses the UNIX *pthread* threading library

### 实验步骤

1. 运行`ph`程序，观察结果

    ```sh
    syy@syyhost:~/xv6-labs-2020$ make ph
    gcc -o ph -g -O2 notxv6/ph.c -pthread
    syy@syyhost:~/xv6-labs-2020$ ./ph 1
    100000 puts, 6.182 seconds, 16177 puts/second
    0: 0 keys missing
    100000 gets, 6.395 seconds, 15637 gets/second
    syy@syyhost:~/xv6-labs-2020$ ./ph 2
    100000 puts, 3.196 seconds, 31287 puts/second       # 变成几乎2倍
    1: 16552 keys missing
    0: 16552 keys missing
    200000 gets, 6.613 seconds, 30244 gets/second
    syy@syyhost:~/xv6-labs-2020$ ./ph 4
    100000 puts, 2.161 seconds, 46284 puts/second       # 可以看到运行4个线程并没有变成4倍(大约3倍)，因为我使用的虚拟机只有两个核
    3: 36451 keys missing
    0: 36451 keys missing
    1: 36451 keys missing
    2: 36451 keys missing
    400000 gets, 11.903 seconds, 33604 gets/second
    ```
    - 在多线程的情况下, 可能出现键值丢失的情况，需要通过加锁来避免

        ```
        [假设键 k1、k2 属于同个 bucket]

        thread 1: 尝试设置 k1
        thread 1: 发现 k1 不存在，尝试在 bucket 末尾插入 k1
        --- scheduler 切换到 thread 2
        thread 2: 尝试设置 k2
        thread 2: 发现 k2 不存在，尝试在 bucket 末尾插入 k2
        thread 2: 分配 entry，在桶末尾插入 k2
        --- scheduler 切换回 thread 1
        thread 1: 分配 entry，没有意识到 k2 的存在，在其认为的 “桶末尾”（实际为 k2 所处位置）插入 k1

        [k1 被插入，但是由于被 k1 覆盖，k2 从桶中消失了，引发了键值丢失]
        ```
    
2. 修改`notxv6/ph.c`, 在put/get操作时添加锁

    ```c
    // ph.c
    pthread_mutex_t lock;

    int
    main(int argc, char *argv[])
    {
        pthread_mutex_init(&lock, NULL);
        // ......
    }

    static 
    void put(int key, int value)
    {
        int i = key % NBUCKET;

        pthread_mutex_lock(&lock);
        
        // ......

        pthread_mutex_unlock(&lock);
    }

    static struct entry*
    get(int key)
    {
        int i = key % NBUCKET;

        pthread_mutex_lock(&lock);
        
        // ......

        pthread_mutex_unlock(&lock);

        return e;
    }
    ```

3. 重新编译并执行多线程版本的ph

    ```sh
    syy@syyhost:~/xv6-labs-2020$ make ph
    gcc -o ph -g -O2 notxv6/ph.c -pthread
    syy@syyhost:~/xv6-labs-2020$ ./ph 2
    100000 puts, 7.877 seconds, 12695 puts/second
    1: 0 keys missing       # 没有missing了
    0: 0 keys missing
    200000 gets, 15.717 seconds, 12725 gets/second
    ```

    - 发现比之前单线程版本还要慢，原因如下：
    
        - 我们为整个操作加上了互斥锁，意味着每一时刻只能有一个线程在操作哈希表，这里实际上等同于将哈希表的操作变回单线程了，
        
        - 又由于锁操作（加锁、解锁、锁竞争）是有开销的，所以性能甚至不如单线程版本。
    
4. 降低锁的粒度， 提高速度

    ```c
    // ph.c
    pthread_mutex_t locks[NBUCKET];

    int
    main(int argc, char *argv[])
    {
        for(int i=0;i<NBUCKET;i++) {
            pthread_mutex_init(&locks[i], NULL); 
        }

        // ......
    }

    static 
    void put(int key, int value)
    {
        int i = key % NBUCKET;

        pthread_mutex_lock(&locks[i]);
        
        // ......

        pthread_mutex_unlock(&locks[i]);
    }

    static struct entry*
    get(int key)
    {
        int i = key % NBUCKET;

        pthread_mutex_lock(&locks[i]);
        
        // ......

        pthread_mutex_unlock(&locks[i]);

        return e;
    }
    ```

    - 重新编译运行

        ```sh
        syy@syyhost:~/xv6-labs-2020$ ./ph 2
        100000 puts, 3.877 seconds, 25792 puts/second   # 比之前的加锁要快
        0: 0 keys missing   # 没有missing
        1: 0 keys missing
        200000 gets, 7.786 seconds, 25686 gets/second
        ```
        - 由于锁开销，依然达不到理想的 单线程速度 * 线程数 那么快

### 实验结果

## Barrier

- 利用 pthread 提供的条件变量方法，实现同步屏障机制: 应用程序中的一个点，所有参与线程必须等待，直到所有其他参与线程也到达该点。

### 实验步骤

1. 编译运行barrier.c

    ```sh
    syy@syyhost:~/xv6-labs-2020$ ./barrier 2
    barrier: notxv6/barrier.c:51: thread: Assertion `i == t' failed.
    barrier: notxv6/barrier.c:51: thread: Assertion `i == t' failed.
    Aborted (core dumped)
    ```


2. 在`notxv6/barrier.c`中添加相关代码, 使得程序正常运行

    ```c
    static void 
    barrier()
    {
        pthread_mutex_lock(&bstate.barrier_mutex);
        if(++bstate.nthread < nthread) {   // 还有线程没有调用barrier()函数
            pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex)
        } else {    // 如果所有线程都到barrier()函数
            bstate.nthread = 0;
            bstate.round++;    // 确保每次i == bstate.round时，两者都调用了barrier()函数
            pthread_cond_broadcast(&bstate.barrier_cond);
        }
        pthread_mutex_unlock(&bstate.barrier_mutex);
    }
    ```
### 实验结果

```sh
syy@syyhost:~/xv6-labs-2020$ ./barrier 2
OK; passed
```

```sh
== Test uthread == 
$ make qemu-gdb
uthread: OK (3.8s) 
== Test answers-thread.txt == answers-thread.txt: FAIL 
    Cannot read answers-thread.txt
== Test ph_safe == make[1]: Entering directory '/home/syy/xv6-labs-2020'
make[1]: 'ph' is up to date.
make[1]: Leaving directory '/home/syy/xv6-labs-2020'
ph_safe: OK (11.5s) 
== Test ph_fast == make[1]: Entering directory '/home/syy/xv6-labs-2020'
make[1]: 'ph' is up to date.
make[1]: Leaving directory '/home/syy/xv6-labs-2020'
ph_fast: OK (24.3s) 
== Test barrier == make[1]: Entering directory '/home/syy/xv6-labs-2020'
make[1]: 'barrier' is up to date.
make[1]: Leaving directory '/home/syy/xv6-labs-2020'
barrier: OK (11.1s) 
== Test time == 
time: FAIL 
    Cannot read time.txt
Score: 54/60
make: *** [Makefile:317: grade] Error 1
```
