# system calls
## System call tracing

- 增加一个trace系统调用。该系统调用有一个int型参数，该参数的每个位表示对应系统调用号的那个系统调用需要被追踪

    ```c
    trace(1 << SYS_fork);       // fork系统调用被追组
    ```

- 如果被追踪的系统调用被调用，则需要打印一行信息，包括:进程ID,这个系统系统调用名字, 返回值

- 调用该系统调用fork后的子进程也需要trace相同的系统调用

- RISV已经提供一个用户空间的`usr/trace.c`程序，因此我们将其变成一个系统调用，这样它才可以修改内核

## 实验步骤

1. 查看`usr/trace.c`程序：
    
    - 调用形式：

        ```sh
        # trace [trace的int型参数] [要执行的程序] [要执行程序的参数]

        trace 32 grep hello README  # 32 = 1 << 5, 因此表示在执行grep程序中，监控系统调用号为5的系统调用的，即read系统调用
        ```

2. 修改`Makefile`文件

    ```sh
    $U/_zombie\
    $U/_trace\
    ```

3. 为用户空间的`usr/trace.c`提供内核空间的程序
    
    > 参数系统调用流程

    1. 在`usr/usy.pl`中，添加相应的系统调用入口声明，该文件会在系统启动后生成的`usr/usys.S`文件中生成相应的汇编指令

        ```sh
        print "#include \"kernel/syscall.h\"\n";
        # ....
        # 指定汇编指令生成的形式
        sub entry {
            my $name = shift;   # name就是entry中的字符串
            print ".global $name\n";
            print "${name}:\n";
            print " li a7, SYS_${name}\n";      # SYS_${name}会根据kernel/syscall.h中宏定义取得相应的系统调用号
            print " ecall\n";       # 调用syscall()函数，根据系统调用号，调用实际的SYS_${name}函数
            print " ret\n";
        }
        #...
        entry("uptime");
        entry("trace");     # 添加  
        ```
    
    2. 在`kernel/syscall.h`中，为`SYS_trace`定义相应的系统调用号

        ```c
        #define SYS_close  21
        #define SYS_trace  22   # 添加
        ```
    
    3. 在特定文件中, 实现`SYS_trace()`

        - 大部分系统调用的函数在`kernel/sysproc.c`中实现;

        - `SYS_trace()`的调用逻辑：使得当前进程在运行过程中，监听某个特定的系统调用。因此我们需要修改进程映像，使得调用该系统调用号，可以记录该进程需要监听的系统调用号

            1. `kernel/proc.h`
        
                ```c
                char name[16];               // Process name (debugging)

                uint64 trace_syscall_number;    // 添加, 记录该进程运行过程中需要追踪的系统调用
                ```

            2. `kernel/sysproc.c`

                ```c
                // 添加函数
                uint64 sys_trace(void)
                {
                        int mask;
                        if(argint(0,&mask) < 0)	// argint获得trace的int型参数
                                return -1; 
                        myproc() -> trace_syscall_number = mask;      // 设置进程需要记录的系统调用号
                        return 0;    
                }
                ```

    4. 在`kernel/sycall.c`中，修改`syscall()`,使得可以根据SYS_trace调用`SYS_trace()`

        ```c
        // 3. 根据mask输出对应系统调用号的名字
        static char *syscall_names[] = {
        [SYS_fork]    "fork",
        [SYS_exit]    "exit",
        // ... 省略
        }
        //...
        extern uint64 sys_uptime(void);
        extern uint64 sys_trace(void);      // 1. 添加函数声明
        //...
        [SYS_close]   sys_close,
        [SYS_trace]   sys_trace,            // 2. 添加系统调用号对应调用的函数
        //...
        p->trapframe->a0 = syscalls[num]();
        if ((p -> syscall_trace >> num) & 1){       // 3.如果此次是该进程需要trace的系统调用，则输出相应信息
            printf("%d: syscall %s -> %d\n", p -> pid,syscall_names[num],p -> trapframe -> a0); 
        } 
        ```
    
    5. 在`usr/user.h`中，给用户态程序添加系统调用跳板函数的声明

        ```c
        int uptime(void);
        int trace(int);
        ```
    
    6. 在`kernel/proc.c`中修改`fork()`, 因为实验要求：由该进程创建的子进程也要监视相同的系统调用

        ```c
        np->state = RUNNABLE;

        np->trace_syscall_number = p->trace_syscall_number;    // 添加，子进程继承trace_syscall_number
        ```
### 结果

```sh
syy@syyhost:~/xv6-labs-2020$ ./grade-lab-syscall trace
make: 'kernel/kernel' is up to date.
== Test trace 32 grep == trace 32 grep: OK (1.2s) 
== Test trace all grep == trace all grep: OK (1.0s) 
== Test trace nothing == trace nothing: OK (1.1s) 
== Test trace children == trace children: OK (10.0s) 
```
### 拓展
Q1：用户程序中，调用`trace()`函数，那怎么就直接执行`usr/usys.S`中相应的汇编指令了呢?

## Sysinfo

- 增加一个sysinfo系统调用，用来收集运行时系统的信息。

- 这个系统调用有一个参数：指向`struct sysinfo`(kernel/sysinfo.h)的指针

- 调用该系统调用，将会填充`sysinfo`结构体中的成员

    ```c
    struct sysinfo {
        uint64 freemem;   // amount of free memory (bytes)
        uint64 nproc;     // number of processes whose state is not UNUSED
    };
    ```

### 实验步骤

1. 修改`Makefile`文件

    ```sh
    $U/_trace\
    $U/_sysinfotest\
    ```
2. 参照上一个实验流程，添加sysinfo系统调用

3. 具体`sys_sysinfo()`函数实现:

    -  needs to copy a struct sysinfo back to user space。
        
        > 因为我们首先是在系统调用中构造sysinfo结构并填写数据，但是我们调用`sysinfo()`是在用户空间，传递的sysinfo结构也是在用户空间，因此需要执行内核空间内存数据拷贝到用户空间映射的内存

        1. `sys_fstat()` (kernel/sysfile.c) and `filestat()` (kernel/file.c) for examples of how to do that using `copyout()`

            ```C
            // filestat()
            // 将内核数据结构stat映射到进程的虚拟地址的用户空间addr中
            if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
                return -1;

            // vm.c - copyout()
            // Copy from kernel to user.
            // Copy len bytes from src to virtual address dstva in a given page table.
            // Return 0 on success, -1 on error.
            int
            copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
            {   
                uint64 n, va0, pa0;

                while(len > 0){
                    va0 = PGROUNDDOWN(dstva);       // PGROUNDDOWN宏, 使得va0以一个页大小为单位的地址
                    pa0 = walkaddr(pagetable, va0); // walkaddr(), 通过页表查找虚拟地址va0对应的物理地址，如果没有映射则返回0
                    if(pa0 == 0)
                        return -1;  // 说明这块虚拟地址空间没有被对应的内存，因此无法保存数据
                    
                    n = PGSIZE - (dstva - va0);     // n此时是一页可用的空间
                    if(n > len)  // 说明这一页就能装下
                        n = len;
                        
                    memmove((void *)(pa0 + (dstva - va0)), src, n); // 复制内容

                    len -= n;       // 如果len还是>0,则说明这一页不够用了
                    src += n;
                    dstva = va0 + PGSIZE;       // 用虚拟地址空间的下一页
                }
                return 0;
            }
            ```

    - To collect the amount of free memory, add a function to `kernel/kalloc.c`

        - 从kernel/kalloc.c中可以知道，RISV采用空闲链表来管理空闲内存

            ```c
            struct {
                struct spinlock lock;
                struct run *freelist;
            } kmem;
            ```

        ```c
        uint64 cal_free_mem(void) {
            struct run *r;
            int free_page_num = 0;
            acquire(&kmem.lock);
            r = kmem.freelist;
            while (r)
            {
                free_page_num++;
                r = r->next;
            }
            release(&kmem.lock);

            return free_page_num * PGSIZE;
        }
        ```
    - To collect the number of processes, add a function to `kernel/proc.c`

        - 从kernel/proc.c中可以知道，所有进程保存在一个数组中

            ```c
            struct proc proc[NPROC];    // NPROC = 64, 最多同时有64个进程
            ```
        
        ```c
        uint cal_not_unused_process_num(void) {
            uint64 cnt = 0;
            for (struct proc *p = proc; p < &proc[NPROC]; p++) {
                if (p->state != UNUSED)
                cnt++;
            }
            return cnt;
        }   
        ```

    - 需要在`kernel/defs.h`添加相关函数的声明

    - 实现`sys_sysinfo()`系统调用(kernel/sysproc.c)

        ```c
        #include "kernel/sysinfo.h"     // 需要添加
        uint sys_sysinfo(void) {
            uint64 user_addr;   // 获得用户程序指定的虚拟地址空间地址
            if(argaddr(0, &user_addr) < 0)
                return -1;

            struct sysinfo sinfo;   // 保存在内核空间的结构
            sinfo.freemem = cal_free_mem();
            sinfo.nproc = cal_not_unused_process_num();

            // 将sinfo的内存内容复制到用户程序指定的虚拟地址空间中
            if(copyout(myproc()->pagetable, user_addr, (char*)&sinfo, sizeof(sinfo)) < 0)
                return -1;
            
            return 0;
        }
        ```
    
### 实验结果

```sh
init: starting sh
$ sysinfotest
sysinfotest: start
sysinfotest: OK
```
    
