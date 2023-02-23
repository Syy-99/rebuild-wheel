# traps

## RISC-V assembly

- 通过回答一些问题，理解RISV的汇编知识

### 实验步骤

- 回答问题：
    1. 寄存器a0-a7用来保存函数参数；

    2. 汇编代码中不存在调用函数f,g的部分，因为都被内联直接计算了
        24: 	4635                    li      a2,13	
        26:   	45b1                    li      a1,12	# 直接计算获得12

    3. printf的地址是0x0000000000000628
        0000000000000628 <printf>:

    4. 因为在RISV中，ra寄存器保存返回地址，所以ra中保存的是jar指令的下一个指令的地址，即0x0000000000000038

    5. %x表示输出的格式为小写的不带前缀的十六进制，%s表示输出一个字符串，因此最终输出时 He110 World。
    如果是大端序，57616不需要修改，因为它就是一个16进制数，i要修改0x726c6400

    6. y是未定义的，会尝试读取寄存器a2的值，而a2寄存器中的值是不确定的

## Backtrace

- 实现一个`backtrace()`(kernel/printf.c.)函数, 该函数打印发生错误时函数调用堆栈保存的地址

- 然后，在`sys_sleep()`中插入对该函数的调用，并在QEMU中运行`bttest`检查结果



### 实验步骤

1. 实现`backtrace()`:

    1. 了解相关知识： [RISV函数调用栈帧结构](https://pdos.csail.mit.edu/6.828/2020/lec/l-riscv-slides.pdf)

        - 编译器将一个执行当前调用者函数的堆栈起始位置(地址)保存在寄存器fp中;

        - 当前调用者栈帧中，fp指向返回地址，fp-8保存了该函数的返回地址，fp-16保存了上一个栈帧的位置

            > 因为RISV是64位，所以一个指针是8字节


        - 在 xv6 中，使用一个页来存储栈，如果 fp 已经到达栈页的上界，则说明已经到达栈底

    2. 利用C内联汇编，获得寄存器r_fp中的值

        ```c
        // kernel/risv.h
        static inline uint64
        r_fp()
        {
            uint64 x;
            asm volatile("mv %0, s0" : "=r" (x) );
            return x;
        }
        ```

    3. 来kernel/printf.c中实现函数

        ```c
        void backtrace() {

            uint64 fp = r_fp();

            while(PGROUNDUP(fp) != fp) {
                uint64 ra = *(uint64*)(fp - 8);		//获得返回地址 
                printf("%p\n", ra);
                fp = *(uint64*)(fp - 16); // 获得上一个堆栈的地址
            }
        }

        ```

        > 这里，我不太理解，为什们输出返回地址？可能是返回堆栈就是这样要求的？

2. 在kernel/defs.h中声明该函数

3. 在`sys_sleep()`第一行调该函数

### 实验结果

```sh
$ bttest
0x0000000080002cbc
0x0000000080002b96
0x0000000080002880

# 退出Qemu后
syy@syyhost:~/xv6-labs-2020$ addr2line -e kernel/kernel
0x0000000080002cbc      # 输入上面的地址
0x0000000080002b96
0x0000000080/home/syy/xv6-labs-2020/kernel/sysproc.c:64
/home/syy/xv6-labs-2020/kernel/syscall.c:140
002880
/home/syy/xv6-labs-2020/kernel/trap.c:82

syy@syyhost:~/xv6-labs-2020$ ./grade-lab-traps backtrace
riscv64-linux-gnu-gcc -Wall -Werror -O -fno-omit-frame-pointer -ggdb -DSOL_TRAPS -DLAB_TRAPS -MD -mcmodel=medany -ffreestanding -fno-common -nostdlib -mno-relax -I. -fno-stack-protector -fno-pie -no-pie   -c -o kernel/printf.o kernel/printf.c
riscv64-linux-gnu-ld -z max-page-size=4096 -T kernel/kernel.ld -o kernel/kernel kernel/entry.o kernel/start.o kernel/console.o kernel/printf.o kernel/uart.o kernel/kalloc.o kernel/spinlock.o kernel/string.o kernel/main.o kernel/vm.o kernel/proc.o kernel/swtch.o kernel/trampoline.o kernel/trap.o kernel/syscall.o kernel/sysproc.o kernel/bio.o kernel/fs.o kernel/log.o kernel/sleeplock.o kernel/file.o kernel/pipe.o kernel/exec.o kernel/sysfile.o kernel/kernelvec.o kernel/plic.o kernel/virtio_disk.o 
riscv64-linux-gnu-ld: warning: cannot find entry symbol _entry; defaulting to 0000000080000000
riscv64-linux-gnu-objdump -S kernel/kernel > kernel/kernel.asm
riscv64-linux-gnu-objdump -t kernel/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$/d' > kernel/kernel.sym
== Test backtrace test == backtrace test: OK (1.6s) 
```

## Alarm 

- In this exercise you'll add a feature to xv6 that periodically alerts a process as it uses CPU time. 

-  More generally, you'll be implementing a primitive form of **user-level interrupt/fault handlers**; you could use something similar to handle page faults in the application, for example.

- 增加一个新的系统调用`sigalarm(interval, handler)`

    1. 如果任意一个程序调用`sigalarm(n,fn)`, 那么之后该程序每使用n个CPU的ticks， 内核就应该调用fn这个函数； 当fn返回，该程序的应该继续运行

        > xv6中，ticks由硬件定时器产生中断的频率决定

    2. If an application calls sigalarm(0, 0), the kernel should stop generating periodic alarm calls.


### 实验步骤

1. 修改Makefile

    ```sh
    $U/_zombie\
    $U/_alarmtest\
    ```
2. 在`user/user.h`中添加相关声明

    ```c
    int sigalarm(int ticks, void (*handler)());
    int sigreturn(void);
    ```

4. 在`struct proc`中添加新的域，来记录该程序占用CPU的ticks数和函数，并在`allocproc()`和`freeproc()`中初始化和释放该域

    ```c
    // kernel/proc.h
    char name[16];               // Process name (debugging)
    
     if((p->alarm_trapframe = (struct trapframe *)kalloc()) == 0){
        release(&p->lock);
        return 0;
    }
    uint64 alarm_interval;    // 记录设置的ticks数
    uint64 alarm_use_ticks;   // 记录该程序还剩的tick数
    void (*alarm_handler)();   // 设置需要执行的函数指针

    // kernel/proc.c -> allocproc() & freeproc()
    if(p->alarm_trapframe)
        kfree((void*)p->alarm_trapframe);
    p->alarm_trapframe = 0;
    p -> alarm_interval = 0;
    p -> alarm_ticks = 0;
    p -> alarm_handler = 0;
    ```

3. 增加`sysalarm`, `sysreturn`系统调用

    > 具体操作略， 类似lab_3, 主要修改的文件有`user/user.pl`, `kernel/syscall.h`, `kernel/syscall.c`, `kernel/sysproc.c`

    ```c
    // kernel/sysproc.c
    uint64 sys_sigalarm(void)
    {
        int n;
        uint64 fn; 
        if(argint(0, &n) < 0)
            return -1; 
        if(argaddr(1,&fn) < 0)
            return -1; 
        return sigalarm(n, (void (*)())(fn));
    }

    uint sys_sigreturn(void)
    {
    return sigreturn();
    }

    // kernel/trap.c
    int sigalarm(int ticks, void(*handler)()) {
        // 设置 myproc 中的相关属性
        struct proc *p = myproc();
        p->alarm_interval = ticks;
        p->alarm_handler = handler;
        p->alarm_use_ticks = ticks;
        return 0;
    }

    int sigreturn() {
        // 将 trapframe 恢复到时钟中断之前的状态，恢复原本正在执行的程序流
        struct proc *p = myproc();
        *p->trapframe = *p->alarm_trapframe;
        p->is_alarm = 0;
        return 0;
    }

    ```

5. 修改`usertrap()`(kernel/trap.c): 因为每个tick，时钟硬件都会发起一个中断;

    时钟中断对应的设备中断号是2

    ```c
    // give up the CPU if this is a timer interrupt.
    if(which_dev == 2) {
        // 如果是一个时钟中断，则说明该进程已经运行了一个tick,需要记录一下
        if (p->alarm_interval != 0) {   // 如果设置了时钟事件
        if (p->is_alarm == 0) {   // 并且当前的时钟是运行该进程而不是alarm_handler
            if (--p->alarm_use_ticks <= 0) {    // // 如果一个时钟到期的时候已经有一个时钟处理函数正在运行，则会推迟到原处理函数运行完成后的下一个 tick 才触发这次时钟
            p->alarm_use_ticks = p->alarm_interval;   // 重新计数

            *p->alarm_trapframe = *p->trapframe;    // 保存一下
            p->trapframe->epc = (uint64)p->alarm_handler;   // 设置trap的返回地址是alarm_handler函数
            p->is_alarm = 1;    // 记录此时是在alarm下运行,在alarm_handler中必须调用sigreturn()将它恢复为0
            }
        }
        }
        yield();
    }

    ```


### 实验结果

```sh
hart 1 starting
init: starting sh
$ alarmtest
test0 start
........alarm!
test0 passed
test1 start
...alarm!
....alarm!
..alarm!
...alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
...alarm!
test1 passed
test2 start
................alarm!
test2 passed
```

```sh
syy@syyhost:~/xv6-labs-2020$ ./grade-lab-traps alarmtest
make: 'kernel/kernel' is up to date.
== Test running alarmtest == (4.1s) 
== Test   alarmtest: test0 == 
  alarmtest: test0: OK 
== Test   alarmtest: test1 == 
  alarmtest: test1: OK 
== Test   alarmtest: test2 == 
  alarmtest: test2: OK 
  ```



