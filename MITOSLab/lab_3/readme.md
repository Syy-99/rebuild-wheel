## Print a page table

- 写一个函数`vmprint()`,接收一个`pagetable_t`参数，根据指定格式打印该页表的内容

- Insert if(p->pid==1) vmprint(p->pagetable) in exec.c just before the return argc, to print the first process's page table

### 实验步骤

1. 在`kernel/vm.c`中实现`vmprint()`

    - 阅读`freewalk()`(kernel/vm.c)

        ```c
        // Recursively free page-table pages.
        // All leaf mappings must already have been removed.
        void
        freewalk(pagetable_t pagetable)
        {
            // 每级页表都有2^9=512个页表项
            for(int i = 0; i < 512; i++){
                pte_t pte = pagetable[i];
                if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){  // 只有最后一级页表设置(PTE_R|PTE_W|PTE_X)
                    // 这个页表项指向下一级别的页表项
                    uint64 child = PTE2PA(pte);
                    freewalk((pagetable_t)child);
                    pagetable[i] = 0;
                } else if(pte & PTE_V){    
                    panic("freewalk: leaf");    // 重点：PTE_V 为 1，说明最后一级页表的页表项映射没取消（取消应该是全0)，会 panic
                }
            }
            kfree((void*)pagetable);      // 释放该页表
        }
        ```
    
    - 编写`vmprint()`

        ```c
        void vmprint_help_func(pagetable_t p, int depth) {
            for (int i = 0; i < 512; i++) {
                pte_t pte = p[i];
                if (pte & PTE_V) {
                printf("..");
                for (int j = 0; j < depth; ++j)
                    printf("  ..");
                printf("%d: pte %p pa %p\n", i, pte, PTE2PA(pte));
                
                if ((pte & (PTE_R|PTE_W|PTE_X)) == 0) { // 这个PTE指向下一层页表
                    uint64 child = PTE2PA(pte);
                    vmprint_help_func((pagetable_t)child, depth + 1);
                }
            }
            }
        }
        void vmprint(pagetable_t p) {
            printf("page table %p\n", p);
            vmprint_help_func(p, 0);

        }

        ```

2. 在kernel/defs.h中添加函数声明

3. 在`kernel/exec.c`的指定位置插入，使用该函数，打印第一个用户进程的页表信息

### 实验结果

```sh
xv6 kernel is booting

hart 2 starting
hart 1 starting
page table 0x0000000087f6e000
..0: pte 0x0000000021fda801 pa 0x0000000087f6a000
..  ..0: pte 0x0000000021fda401 pa 0x0000000087f69000
..  ..  ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000
..  ..  ..1: pte 0x0000000021fda00f pa 0x0000000087f68000
..  ..  ..2: pte 0x0000000021fd9c1f pa 0x0000000087f67000
..255: pte 0x0000000021fdb401 pa 0x0000000087f6d000
..  ..511: pte 0x0000000021fdb001 pa 0x0000000087f6c000
..  ..  ..510: pte 0x0000000021fdd807 pa 0x0000000087f76000
..  ..  ..511: pte 0x0000000020001c0b pa 0x0000000080007000
init: starting sh
$ 
```

## A kernel page table per process 
> - 无论何时在内核执行时，xv6使用同一个内核页表。内核页表是一个物理地址的直接映射，因此内核虚拟地址x对应物理地址x。
> - xv6也有一个单独的页表给每个进程的用户地址空间，**仅包含那个进程用户内存的映射**，起始于虚拟地址0。
> - 因为内核页表不包含这些映射，用户地址在内核无效。
> - 因此，当内核需要使用一个用户指针传到system call时，内核必须首先翻译指针到物理地址。
>- 这个和下个实验的目的是为了允许内核直接解析用户指针。

- 修改内核，每个进程当在内核空间执行时，使用自己的内核页表

- Modify `struct proc` to maintain a kernel page table for each process, and modify the *scheduler* to switch kernel page tables when switching processes

### 实验步骤

1. 在`kernel/proc.h`中的进程结构中`struct proc`添加一个新的成员

    ```c
    pagetable_t pagetable;       // User page table
    pagetable_t kernel_pagetable;   // 每个用户进程自己的内核页表
    ```
2. 修改`kernel/vm.c`中对内核页表进行映射的相关函数

    ```c
    void kvm_map_pagetable(pagetable_t pgtbl) {
        // 将各种内核需要的 direct mapping 添加到页表 pgtbl 中。
        // uart registers
        kvmmap(pgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

        // virtio mmio disk interface
        kvmmap(pgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

        // CLINT
        kvmmap(pgtbl, CLINT, CLINT, 0x10000, PTE_R | PTE_W);

        // PLIC
        kvmmap(pgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

        // map kernel text executable and read-only.
        kvmmap(pgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

        // map kernel data and the physical RAM we'll make use of.
        kvmmap(pgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

        // map the trampoline for trap entry/exit to
        // the highest virtual address in the kernel.
        kvmmap(pgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
    }

    // 添加
    pagetable_t
    kvminit_newpgtbl()    // 创建一个内核页表
    {
        pagetable_t pgtbl = (pagetable_t) kalloc();
        memset(pgtbl, 0, PGSIZE);

        kvm_map_pagetable(pgtbl);

        return pgtbl;
    }

    /*
    * create a direct-map page table for the kernel. 创建内核页表
    */
    // 修改
    void
    kvminit()
    {
        kernel_pagetable =  kvminit_newpgtbl();    // 创建全局共享的内核页表(只需要一页即可)，用于内核 boot 过程
    }

    // 修改
    void
    kvmmap(pagetable_t pgtbl,uint64 va, uint64 pa, uint64 sz, int perm) // 原始的kvmmap默认对唯一的内核页表kernel_pagetable进映射
    {
        if(mappages(pgtbl, va, sz, pa, perm) != 0)
            panic("kvmmap");
    }

    // 修改
    uint64
    kvmpa(pagetable_t pgtbl,uint64 va)       // 根据内核页表，获得虚拟地址va对应的物理地址
    {
        uint64 off = va % PGSIZE;
        pte_t *pte;
        uint64 pa;
        
        pte = walk(pgtbl, va, 0);     // 原始的kvmpa默认对唯一的内核页表kernel_pagetable进映射
        if(pte == 0)
            panic("kvmpa");
        if((*pte & PTE_V) == 0)
            panic("kvmpa");
        pa = PTE2PA(*pte);
        return pa+off;
    }

    ```

3. 修改内核页表对内核栈的映射

    - 在原始设计中，所有进程共享一个内核页表，但是每个进程都有自己的内核栈，因此全局的内核页表中映射有多个内核栈

    - xv6 在启动过程中，会在 procinit() 中为所有可能的 64 个进程位都预分配好内核栈 kstack，具体为在高地址空间里，每个进程使用一个页作为 kstack，并且两个不同 kstack 中间隔着一个无映射的 guard page 用于检测栈溢出错误

    - 现在每个进程一个内核页表，因此中每个进程的内核页表只需要映射一个内核栈即可

    ```c
    // 为每个用户进程分配一个内核栈
    // 该内核栈将被映射到内核虚拟地址空间的高地址部分，位于trampoline下方
    // initialize the proc table at boot time.
    // 修改
    void
    procinit(void)
    {
    struct proc *p;
    
    initlock(&pid_lock, "nextpid");
    for(p = proc; p < &proc[NPROC]; p++) {
        initlock(&p->lock, "proc");

        // 这里删除了为所有进程预分配内核栈的代码，变为创建进程的时候再创建内核栈，见 allocproc(
        // Allocate a page for the process's kernel stack.
        // Map it high in memory, followed by an invalid
        // guard page.
        // char *pa = kalloc();
        // if(pa == 0)
        //   panic("kalloc");
        // // 指针相减就是地址相减，获取当前进程p和proc数组最开始位置的偏移量
        // // 比如第一次，从p-proc=0开始，KSTACK生成虚拟地址: TRAMPOLINE - 2*PGSIZE
        // // 因此TRAMPOLINE的下面第一页是guard page，第二页是kstack，也就是va指向的位置
        // // 后面也以此类推，被跳过而未被处理的guard page，PTE_V是无效的
        // uint64 va = KSTACK((int) (p - proc));
        // kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
        // p->kstack = va;
    }
        kvminithart();    // 将更新后的内核页表重新写入到satp中
    }
    ```

    ```c
    // kernel/proc.c --- allocproc()
    // An empty user page table.
    p->pagetable = proc_pagetable(p);
    if(p->pagetable == 0){
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // 添加-----
    p->kernel_pagetable = kvminit_newpgtbl();

    // 分配一个物理页，作为新进程的内核栈使用
    char *pa = kalloc();
    if(pa == 0)
        panic("kalloc");
    uint64 va = KSTACK((int)0); // 将内核栈映射到固定的逻辑地址上
    // printf("map krnlstack va: %p to pa: %p\n", va, pa);
    kvmmap(p->kernel_pagetable, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    p->kstack = va; // 记录内核栈的逻辑地址，其实已经是固定的了，依然这样记录是为了避免需要修改其他部分 xv6 代码
    //----

    // Set up new context to start executing at forkret,
    // which returns to user space.
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64)forkret;
    p->context.sp = p->kstack + PGSIZE;
    ```

4. 修改调度器`scheduler()`(kernel/proc.c), 使得用户进程在切换到内核空间执行时，使用自己的内核栈

    ```C
     c->proc = p;

    // 切换到进程独立的内核页表
    w_satp(MAKE_SATP(p->kernel_pagetable));
    sfence_vma(); // 清除快表缓存

    swtch(&c->context, &p->context);

    // 切换回全局内核页表
    kvminithart();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
    ```

5. 修改`freeproc()`(kernel/proc.c), 使得在进程结束后，释放进程独享的页表以及内核栈

    ```c
    p->state = UNUSED;

    // 添加
    // 释放内核页表，但是除了内核栈之外的物理页都不应该被是释放

    // 单独处理：释放内核栈对应的物理页
    void *kstack_pa = (void*)kvmpa(p->kernel_pagetable, p->kstack);
    kfree(kstack_pa);
    p->kstack = 0;

    // 统一处理：释放内核页表
    kvm_free_kernelpgtbl(p->kernel_pagetable);
    p->kernel_pagetable = 0;
    //----------------
    p->state = UNUSED;
    ```

    - 内核栈是属于每个进程的，所以该进程退出时，它对应的内核栈使用的内存可以释放

    - 内核页表其他映射都是被共享的，所以对应内存不能被释放，只能释放相应的内核页表,添加`kvm_free_kernelpgtbl()`(kernel.vm.c)

        ```c
        // 类似freewalk，但是该函数不需要强制要求最后一级的页表项的映射被取消
        // 所以少了一个判断
        void
        kvm_free_kernelpgtbl(pagetable_t pagetable)
        {
        // there are 2^9 = 512 PTEs in a page table.
            for(int i = 0; i < 512; i++){
                pte_t pte = pagetable[i];
                uint64 child = PTE2PA(pte);
                if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){ // 如果该页表项指向更低一级的页表
                // 递归释放低一级页表及其页表项
                kvm_free_kernelpgtbl((pagetable_t)child);
                pagetable[i] = 0;
                }
            }
            kfree((void*)pagetable); // 释放当前级别页表所占用空间
        }
        ```

### 实验结果

- 编译出错
    ```sh
    kernel/virtio_disk.c: In function ‘virtio_disk_rw’:
    kernel/virtio_disk.c:206:43: error: passing argument 1 of ‘kvmpa’ makes pointer from integer without a cast [-Werror=int-conversion]
    206 |   disk.desc[idx[0]].addr = (uint64) kvmpa((uint64) &buf0);
        |                                           ^~~~~~~~~~~~~~
        |                                           |
        |                                           long unsigned int
    In file included from kernel/virtio_disk.c:11:
    ```
     - 修改相应程序
        
        ```c
        // virtio_disk.c
        #include "proc.h" // 添加头文件引入

        // ......

        void
        virtio_disk_rw(struct buf *b, int write)
        {
            // ......
            disk.desc[idx[0]].addr = (uint64) kvmpa(myproc()->kernelpgtbl, (uint64) &buf0); // 调用 myproc()，获取进程内核页表
            // ......
        }

- 最终结果：
```sh
test bigdir: OK
ALL TESTS PASSED
```
