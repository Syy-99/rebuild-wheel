# xv6 lazy page allocation

-  Xv6 applications ask the kernel for heap memory using the `sbrk()` system call.

- `sbrk()` doesn't allocate physical memory, but just remembers which user addresses are allocated and marks those addresses as invalid in the user page table. 

- When the process first tries to use any given page of lazily-allocated memory, the CPU generates a page fault, which the kernel handles by allocating physical memory, zeroing it, and mapping it

## Eliminate allocation from sbrk()

- delete page allocation from the `sbrk(n)` system call implementation, which is the function `sys_sbrk()` in sysproc.c.

- Your new sbrk(n) should just increment the process's size (myproc()->sz) by n and return the old size. It should not allocate memory -- so you should delete the call to growproc() (but you still need to increase the process's size!).

- sbrk(n)只增加虚拟地址，删除sbrk(n)实际分配物理内存的操作

### 实验步骤

- 修改`sbrk()`(kernel/proc.c)的实现：

    ```c
    uint64
    sys_sbrk(void)
    {
        int addr;
        int n;
        struct proc *p = myproc();
        if(argint(0, &n) < 0)
            return -1;
        addr = p->sz;
        if(n < 0) {
            uvmdealloc(p->pagetable, p->sz, p->sz+n); // 如果是缩小空间，则马上释放
        }
        p->sz += n; // 懒分配
        return addr;
    }
    ```
    - 参考`growproc()`修改

### 实验结果

```sh
init: starting sh
$ echo hi
usertrap(): unexpected scause 0x000000000000000f pid=3
            sepc=0x00000000000012a8 stval=0x0000000000004008
panic: uvmunmap: not mapped
```

## Lazy allocation

- Modify the code in trap.c to respond to a page fault from user space by mapping a newly-allocated page of physical memory at the faulting address, and then returning back to user space to let the process continue executing. 

- 处理来自用户的缺页错误Page-fault

### 实验步骤

1. 修改`usertrap()`(kernel/trap.c)

    ```c
    if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
    } else if((which_dev = devintr()) != 0){
        // ok
    } else {

        if((r_scause() == 13 || r_scause() == 15) && uvmshouldtouch(va)){ // 缺页异常，并且发生异常的地址进行过懒分配
            uvmlazytouch(va); // 分配物理内存，并在页表创建映射
        } else { 
            printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
            printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
            p->killed = 1;
        }
    }   
    ```
    - 这里之所以提取成函数，是因为后续还会用到这部分操作

    - 注意这里只处理了13和15，对应加载指令和存储指令;
    
      > scause=12的错误是针对执行指令，为什们不用处理呢？

- 修改`uvmunmap()`(kernel/vm.c)

    ```c
    if((*pte & PTE_V) == 0)
      continue;         // 之前是panic, 现在有lazy allocation, 有可能出现对应的页表的PTE时无效的情况
    ```

### 实验结果

```sh
init: starting sh
$ echo hi
hi
```

## Lazytests and Usertests

- 修改lazy memeroy allocator, 以通过`layztests.c` 和 `usertests.c`

- 实际上就是完善实验2的代码

### 实验步骤

1. 运行`lazytests.c`:

    ```sh
    $ lazytests
    lazytests starting
    running test lazy alloc
    panic: uvmunmap: walk
    QEMU: Terminated
    ```

    - 查看`lazytests.c`, 发现第一个测试使用sbrk()申请了1GB的虚拟内存，然后访问，变回出现PTE不存在的情况，因此还需要修改`uvmunmap()`

        ```c
        if((pte = walk(pagetable, a, 0)) == 0)
            continue;
        ```

        > 对于 walk()返回==0 的情况, 一般是 sbrk() 申请了较大内存, L2 或 L1 中的 PTE 就未分配, 致使 L0 页目录就不存在, 虚拟地址对应的 PTE 也就不存在; 
        >
        > 而对于之前提到的 (*pte&PTE_V)==0 的情况, 一般是 sbrk() 申请的内存不是特别大, 当前分配的 L0 页目录还有效, 但虚拟地址对应 L0 中的 PTE 无效. 
        >
        > 这两种情况都是 Lazy Allocation 未实际分配内存所产生的情况, 因此在取消映射时都应该跳过而非 panic.
2. 运行

    ```sh
    $ lazytests
    lazytests starting
    running test lazy alloc
    test lazy alloc: OK
    running test lazy unmap
    panic: uvmcopy: page not present
    ```

    - 经过查看，发现第2个测试是关于fork(),需要修改`uvmcopy()`函数
        
        ```c
        if((pte = walk(old, i, 0)) == 0)
            continue;
            // panic("uvmcopy: pte should exist");
        if((*pte & PTE_V) == 0)
            continue;
            // panic("uvmcopy: page not present");
      ```

      - uvmcopy()拷贝父进程的页表项到子进程的页表中，同样处理因为lazy allocation而特殊的PTE

      > 注意，此时的va都是在p->sz下面，都是该进程可以使用的虚拟地址，所以都需要遍历处理
      >
      > 这样, 即使该va对应的PTE不存在，或者无效，也不会panic

3. 修改copyin() 和 copyout()：内核/用户态之间互相拷贝数据

    - 由于这里可能会访问到懒分配但是还没实际分配的页，所以要加一个检测，确保 copy 之前，用户态地址对应的页都有被实际分配和映射。

        ```c
        // 修改这个解决了 read/write 时的错误 (usertests 中的 sbrkarg 失败的问题)
        int
        copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
        {
            uint64 n, va0, pa0;

            if(uvmshouldtouch(dstva))
                uvmlazytouch(dstva);

            // ......

        }

        int
        copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
        {
            uint64 n, va0, pa0;

            if(uvmshouldtouch(srcva))
                uvmlazytouch(srcva);

            // ......

        }
        ```

### 实验结果

```sh
$ lazytests
lazytests starting
running test lazy alloc
test lazy alloc: OK
running test lazy unmap
usertrap(): unexpected scause 0x000000000000000f pid=7
            sepc=0x0000000000000130 stval=0x0000000000004000
#...
test bigdir: OK
lazy alloc: out of memory
ALL TESTS PASSED
$ QEMU: Terminated
```

> tip. 如果 usertests 某一步失败了，可以用 usertests [测试名称] 直接单独运行某个之前失败过的测试，例如 usertests stacktest 可以直接运行栈 guard page 的测试，而不用等待其他测试漫长的运行。

```sh
== Test running lazytests == 
$ make qemu-gdb
(6.7s) 
== Test   lazy: map == 
  lazy: map: OK 
== Test   lazy: unmap == 
  lazy: unmap: OK 
== Test usertests == 
$ make qemu-gdb
(172.2s) 
== Test   usertests: pgbug == 
  usertests: pgbug: OK 
== Test   usertests: sbrkbugs == 
  usertests: sbrkbugs: OK 
== Test   usertests: argptest == 
  usertests: argptest: OK 
== Test   usertests: sbrkmuch == 
  usertests: sbrkmuch: OK 
== Test   usertests: sbrkfail == 
  usertests: sbrkfail: OK 
== Test   usertests: sbrkarg == 
  usertests: sbrkarg: OK 
== Test   usertests: stacktest == 
  usertests: stacktest: OK 
== Test   usertests: execout == 
  usertests: execout: OK 
== Test   usertests: copyin == 
  usertests: copyin: OK 
== Test   usertests: copyout == 
  usertests: copyout: OK 
== Test   usertests: copyinstr1 == 
  usertests: copyinstr1: OK 
== Test   usertests: copyinstr2 == 
  usertests: copyinstr2: OK 
== Test   usertests: copyinstr3 == 
  usertests: copyinstr3: OK 
== Test   usertests: rwsbrk == 
  usertests: rwsbrk: OK 
== Test   usertests: truncate1 == 
  usertests: truncate1: OK 
== Test   usertests: truncate2 == 
  usertests: truncate2: OK 
== Test   usertests: truncate3 == 
  usertests: truncate3: OK 
== Test   usertests: reparent2 == 
  usertests: reparent2: OK 
== Test   usertests: badarg == 
  usertests: badarg: OK 
== Test   usertests: reparent == 
  usertests: reparent: OK 
== Test   usertests: twochildren == 
  usertests: twochildren: OK 
== Test   usertests: forkfork == 
  usertests: forkfork: OK 
== Test   usertests: forkforkfork == 
  usertests: forkforkfork: OK 
== Test   usertests: createdelete == 
  usertests: createdelete: OK 
== Test   usertests: linkunlink == 
  usertests: linkunlink: OK 
== Test   usertests: linktest == 
  usertests: linktest: OK 
== Test   usertests: unlinkread == 
  usertests: unlinkread: OK 
== Test   usertests: concreate == 
  usertests: concreate: OK 
== Test   usertests: subdir == 
  usertests: subdir: OK 
== Test   usertests: fourfiles == 
  usertests: fourfiles: OK 
== Test   usertests: sharedfd == 
  usertests: sharedfd: OK 
== Test   usertests: exectest == 
  usertests: exectest: OK 
== Test   usertests: bigargtest == 
  usertests: bigargtest: OK 
== Test   usertests: bigwrite == 
  usertests: bigwrite: OK 
== Test   usertests: bsstest == 
  usertests: bsstest: OK 
== Test   usertests: sbrkbasic == 
  usertests: sbrkbasic: OK 
== Test   usertests: kernmem == 
  usertests: kernmem: OK 
== Test   usertests: validatetest == 
  usertests: validatetest: OK 
== Test   usertests: opentest == 
  usertests: opentest: OK 
== Test   usertests: writetest == 
  usertests: writetest: OK 
== Test   usertests: writebig == 
  usertests: writebig: OK 
== Test   usertests: createtest == 
  usertests: createtest: OK 
== Test   usertests: openiput == 
  usertests: openiput: OK 
== Test   usertests: exitiput == 
  usertests: exitiput: OK 
== Test   usertests: iput == 
  usertests: iput: OK 
== Test   usertests: mem == 
  usertests: mem: OK 
== Test   usertests: pipe1 == 
  usertests: pipe1: OK 
== Test   usertests: preempt == 
  usertests: preempt: OK 
== Test   usertests: exitwait == 
  usertests: exitwait: OK 
== Test   usertests: rmdot == 
  usertests: rmdot: OK 
== Test   usertests: fourteen == 
  usertests: fourteen: OK 
== Test   usertests: bigfile == 
  usertests: bigfile: OK 
== Test   usertests: dirfile == 
  usertests: dirfile: OK 
== Test   usertests: iref == 
  usertests: iref: OK 
== Test   usertests: forktest == 
  usertests: forktest: OK 
== Test time == 
time: FAIL 
    Cannot read time.txt
Score: 118/119
make: *** [Makefile:316: grade] Error 1
syy@syyhost:~/xv6-labs-2020$ 
```