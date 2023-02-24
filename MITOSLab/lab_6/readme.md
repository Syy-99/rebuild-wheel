# Copy-on-Write Fork for xv6

## Implement copy-on write

- The goal of copy-on-write (COW) fork() is to **defer allocating and copying physical memory pages for the child** until the copies are actually needed, if ever.

- COW fork() **creates just a pagetable for the child**, with PTEs for user memory **pointing to the parent's physical pages**

- A given physical page may be referred to by multiple processes' page tables, and should be freed only when the last reference disappears.

    > COW使得释放用户的物理内存变得更复杂，一个物理页面可能被多个进程的页表所引用
    > 必须在最后一个引用消失时才能释放


### 实验步骤

1. 阅读原始的`fork()`(kernel/proc.c)实现

    ```c
    // Copy user memory from parent to child.
    if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
        freeproc(np);
        release(&np->lock);
        return -1;
    }
    ```

    - fork()利用`uvmcopy()`实现物理页面的复制，因此需要修改uvmcopy()

2. 考虑需要对物理页进行引用计数，以确保在还有引用的情况下，就将该物理页释放

    - 因为可用的物理内存是有限的[KERNBASE, PHYSTOP], 对应的物理页数量也是有限的，因此可以使用一个全局的数组来记录对应物理页的引用计数

    - 因为是全局的数组，所以在修改时，需要加锁访问

    ```c
    //kernel/kalloc.c
    
    // 用于访问物理页引用计数数组
    #define PA2PGREF_ID(p) (((p)-KERNBASE)/PGSIZE)    // 获得物理地址p对应的页编号
    #define PGREF_MAX_ENTRIES PA2PGREF_ID(PHYSTOP)    // 最大物理地址对应的页编号

    // 从 KERNBASE 开始到 PHYSTOP 之间的每个物理页的引用计数
    int pageref[PGREF_MAX_ENTRIES];
    // 访问pageref数组的锁，因为该数组是一个全局变量，可能同时有多个进程修改它的值
    struct spinlock pgreflock;

    // 通过物理地址获得对应物理页的引用计数
    #define PA2PGREF(p) pageref[PA2PGREF_ID((uint64)(p))]

    void
    kinit()
    {
        initlock(&kmem.lock, "kmem");
        // 内核启动时，初始化pageref数组的锁
        initlock(pgreflock,"pageref");
        freerange(end, (void*)PHYSTOP);
    }

    void
    kfree(void *pa)
    {
        //...
        // 每次想要释放物理地址pa对应的页时，就需要判断该页的引用计数，来决定是否真正释放
        acquire(&pgreflock);
        if (--PA2PGREF(pa) <= 0) {
            // Fill with junk to catch dangling refs.
            memset(pa, 1, PGSIZE);
            r = (struct run*)pa;

            acquire(&kmem.lock);
            r->next = kmem.freelist;
            kmem.freelist = r;
            release(&kmem.lock);
        }
        release(&pgreflock);
    }

    void *
    kalloc(void) 
    {
        if(r) {
            memset((char*)r, 5, PGSIZE); // fill with junk
            // 新分配的物理页的引用计数为 1
            // (这里无需加锁),因为此时不可能有其他有其他访问
            PA2PGREF(r) = 1;
        } 
    }

    // 为 pa 的引用计数增加 1
    void krefpage(void *pa) {
        acquire(&pgreflock);
        PA2PGREF(pa)++;
        release(&pgreflock);
    }
    ```
        - kalloc()可以不用加锁: 该函数已经通过加锁，确保同一个物理页不会同时被两个进程分配，因此不可能有两个进程同时操作这块物理页

2. 修改`uvmcopy()`(kernel/vm.c)

    ```c
    for(i = 0; i < sz; i += PGSIZE){
        if((pte = walk(old, i, 0)) == 0)
            panic("uvmcopy: pte should exist");
        if((*pte & PTE_V) == 0)
            panic("uvmcopy: page not present");

        pa = PTE2PA(*pte);

        // 不分配物理页
        // if((mem = kalloc()) == 0)
        //   goto err;
        // memmove(mem, (char*)pa, PGSIZE);

        if (*pte & PTE_W)   // 如果物理页可写，则要清楚，并且标记为共享物理页
            *pte = (*pte & ~PTE_W) | PTE_COW;
        
        flags = PTE_FLAGS(*pte);

        if(mappages(new, i, PGSIZE, (uint64)pa, flags) != 0){     // 新的页表中的pte映射同一块物理页pa
            kfree(mem);
            goto err;
        }
        
        // 增加该物理页的引用计数
        krefpage((void*)pa);
    }
    ```

    - `PTE_COW`: 用于标示一个映射对应的物理页是否被共享
    
        ```c
        // kernel/risv.h
        #define PTE_COW (1L << 8)   // 页表项flags中，第8、9、10 位均为保留给操作系统使用的位，可以用作任意自定义用途）
        ```
    - 先在uvmcopy实现了懒分配
3. 修改`usertrap()`(kernel/trap.c)中，处理对应异常

    ```c
    if ((r_scause() == 15) && uvmcheckcowpage(r_stval())) {
    // if ((r_scause() == 15) && uvmcheckcowpage(r_stval())) {
      if (uvmcopycowpage(r_stval()) == -1)
        p->killed = 1;  // 内存不做，无法进行COW则杀死进程
    } else {
      printf("%d",r_scause());
      printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
      printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
      p->killed = 1;
    }
    ```
    - 提供辅助函数

        ```c
        // 检查一个地址指向的页是否是懒复制页
        int uvmcheckcowpage(uint64 va) {
            pte_t *pte;
            struct proc *p = myproc();
            
            return va < p->sz // 在进程内存范围内
                && ((pte = walk(p->pagetable, va, 0))!=0)
                && (*pte & PTE_V) // 页表项存在
                && (*pte & PTE_COW); // 页是一个懒复制页
        }

        // 拷贝va映射的共享物理页
        int uvmcopycowpage(uint64 va) {
            pte_t *pte;
            struct proc *p = myproc();

            if((pte = walk(p->pagetable, va, 0)) == 0)
                panic("uvmcopy: pte should exist");

            uint64 pa = PTE2PA(*pte);

            // 这里将操作用函数封装，主要是需要使用一些kerne/kalloc.c的物理页引用数组
            uint64 new = (uint64)kcopy_n_deref((void*)pa);
            if (new == 0)
                return -1;
            
            // 修改PTE的flag, 新分配的物理页对应的新PTE的PTE_COW标记
            uint64 flags = (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW;
            // 用新的flags重新映射
            uvmunmap(p->pagetable, PGROUNDDOWN(va), 1, 0);
            if(mappages(p->pagetable, va, 1, new, flags) == -1) {
                panic("uvmcowcopy: mappages");
            }
                return 0;
            
        }
        ```
5. 修改`copyout()`:

    ```c
    if(uvmcheckcowpage(dstva)) // 检查每一个被写的页是否是 COW 页
      uvmcopycowpage(dstva);    // 如果是，则需要进行实际的分配动作

    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0)
    ```

    - 必须确保在执行复制动作的时候，对应的物理页面是存在的

### 实验结果
```sh
 cowtest
simple: ok
simple: ok
three: ok
three: ok
three: ok
file: ok
ALL COW TESTS PASSED
$ usertests
usertests starting
test execout: OK
test copyin: OK
#...
ALL TESTS PASSED
```

```sh
== Test running cowtest == 
$ make qemu-gdb
(9.1s) 
== Test   simple == 
  simple: OK 
== Test   three == 
  three: OK 
== Test   file == 
  file: OK 
== Test usertests == 
$ make qemu-gdb
(150.7s) 
== Test   usertests: copyin == 
  usertests: copyin: OK 
== Test   usertests: copyout == 
  usertests: copyout: OK 
== Test   usertests: all tests == 
  usertests: all tests: OK 
== Test time == 
time: FAIL 
    Cannot read time.txt
Score: 109/110
make: *** [Makefile:316: grade] Error 1
```