# Lab: locks

## Memory allocator

- The root cause of lock contention in kalloctest is that kalloc() has a single free list, protected by a single lock 

- To remove lock contention, you will have to redesign the memory allocator to avoid a single lock and list. 

- Your job is to implement per-CPU freelists, and stealing when a CPU's free list is empty.

    > 每个CPU一个空闲链表，并且当某个CPU的空闲链表为空时，可以从其他CPU的空闲链表中“偷”一部分内存

### 实验步骤

1. 运行`kalloctest`程序, 查看多个进程对`kalloc()`中锁的竞争情况

    ```sh
    $ kalloctest
    start test1
    test1 results:
    --- lock kmem/bcache stats  
    lock: kmem: #fetch-and-add 4262184 #acquire() 433016
    lock: bcache: #fetch-and-add 0 #acquire() 1254
    --- top 5 contended locks:
    lock: kmem: #fetch-and-add 4262184 #acquire() 433016
    lock: proc: #fetch-and-add 745976 #acquire() 157046
    lock: proc: #fetch-and-add 520426 #acquire() 157046
    lock: proc: #fetch-and-add 493300 #acquire() 157046
    lock: proc: #fetch-and-add 454786 #acquire() 157046
    tot= 4262184
    test1 FAIL
    start test2
    total free number of pages: 32499 (out of 32768)
    ```
    -  `#fetch-and-add 4262184 #acquire() 433016`：表示acquire()被调用了433016次，且自旋尝试获取锁的次数为 4262184 次
    
    - Q: 这个输出是在哪部分代码呢?
    
        - 经过查找，这部分的输出和`kalloc()/kfree()`中的`acquire()`有关，
        
        ```c
        // acquire(): Acquire the lock.
        // Loops (spins) until the lock is acquired.
        //...省略部分代码
        #ifdef LAB_LOCK
            __sync_fetch_and_add(&(lk->n), 1);
        #endif      

        // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
        //   a5 = 1
        //   s1 = &lk->locked
        //   amoswap.w.aq a5, a5, (s1)
        while(__sync_lock_test_and_set(&lk->locked, 1) != 0) {
        #ifdef LAB_LOCK
            __sync_fetch_and_add(&(lk->nts), 1);
        #else
        ;
        #endif
        }
        ```
  
- 根据实验指导，修改`kernel/kalloc.c`的设计，使得每一个CPU各有一个freelist，降低锁粒度，减少锁竞争

    - 参考该目录下对应代码文件


### 实验结果

```sh
init: starting sh
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem_cpu_0: #fetch-and-add 0 #acquire() 165921
lock: kmem_cpu_1: #fetch-and-add 0 #acquire() 153327
lock: kmem_cpu_2: #fetch-and-add 0 #acquire() 113773
lock: bcache: #fetch-and-add 0 #acquire() 1254
--- top 5 contended locks:
lock: proc: #fetch-and-add 608749 #acquire() 138073
lock: proc: #fetch-and-add 371004 #acquire() 138073
lock: proc: #fetch-and-add 337457 #acquire() 138073
lock: proc: #fetch-and-add 253980 #acquire() 138161
lock: proc: #fetch-and-add 250479 #acquire() 138073
tot= 0
test1 OK
start test2
total free number of pages: 32499 (out of 32768)
.....
test2 OK

$ usertests
usertests starting
#....
test forktest: OK
test bigdir: OK
ALL TESTS PASSED
```

## Buffer cache

- 实验说明和上述类似

- Modify the block cache so that the number of acquire loop iterations for all locks in the bcache is close to zero when running `bcachetest`.
    
    - 理想情况是0，小于500即可

- 实际也是降低锁粒度，当修改不同的缓存时，不需要锁，而不是一使用`bget()`就加锁；

- You must maintain the invariant that at most one copy of each block is cached

### 实验步骤

1. 运行原始`bcachetest.c`程序：

    ```sh
    $ bcachetest
    start test0
    test0 results:
    --- lock kmem/bcache stats
    lock: kmem_cpu_0: #fetch-and-add 0 #acquire() 737501
    lock: kmem_cpu_1: #fetch-and-add 0 #acquire() 485291
    lock: kmem_cpu_2: #fetch-and-add 0 #acquire() 775657
    lock: bcache: #fetch-and-add 1705142 #acquire() 1761312
    --- top 5 contended locks:
    lock: proc: #fetch-and-add 82971680 #acquire() 7051358
    lock: proc: #fetch-and-add 68461737 #acquire() 7047450
    lock: proc: #fetch-and-add 52011070 #acquire() 7037880
    lock: proc: #fetch-and-add 44227293 #acquire() 7042502
    lock: proc: #fetch-and-add 38017266 #acquire() 7042100
    tot= 1705142
    test0: FAIL
    start test1
    test1 OK
    ```

    - 检查这些输出的来源： `statslock()`(kernel/spinlock.c)

2. 修改`bio.c`的实现

    - 注意： 不像kalloc中一个物理页分配后就只归单个进程所管，bcache中的区块缓存是会**被多个进程（进一步地，被多个 CPU）共享的（由于多个进程可以同时访问同一个区块）**。所以 kmem 中为每个 CPU 预先分割一部分专属的页的方法在这里是行不通的。
            
        > - We suggest you look up block numbers in the cache with a hash table that **has a lock per hash bucket**
        >
        > - It is OK to use a fixed number of buckets and not resize the hash table dynamically. Use a prime number of buckets (e.g., 13) to reduce the likelihood of hashing conflicts.
        >
        > - Searching in the hash table for a buffer and allocating an entry for that buffer when the buffer is not found must be atomic.

    - 这部分的实现细节，可以**参考[第一个链接](https://blog.miigon.net/posts/s081-lab8-locks)**，有更深入的分析

### 实验结果

```sh
$ bcachetest
start test0
test0 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 33033
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 6392
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 6672
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 6686
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 7002
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 6268
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 4202
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 4202
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 2194
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 4212
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 2196
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 4360
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 4398
lock: bcache_bufmap: #fetch-and-add 0 #acquire() 6408
--- top 5 contended locks:
lock: virtio_disk: #fetch-and-add 7044078 #acquire() 1101
lock: proc: #fetch-and-add 626031 #acquire() 92095
lock: proc: #fetch-and-add 555696 #acquire() 92095
lock: proc: #fetch-and-add 518981 #acquire() 92095
lock: proc: #fetch-and-add 415970 #acquire() 92095
tot= 0
test0: OK
start test1
test1 OK
```

- 这部分要多运行几次，才能确保大概率正确

### 实验拓展

Q: 锁竞争优化一般有几个思路：

1. 只在必须共享的时候共享（对应为将资源从 CPU 共享拆分为每个 CPU 独立）

2. 必须共享时，尽量减少在关键区中停留的时间（对应“大锁化小锁”，降低锁的粒度）