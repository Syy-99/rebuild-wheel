// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];   // 修改：每个CPU对应一个Kmem

// 每个Kmem的锁的名字
// 注意，实验要求必须以kmem开头; 并且NCPU=8
char *kmem_lock_names[] = {
  "kmem_cpu_0",
  "kmem_cpu_1",
  "kmem_cpu_2",
  "kmem_cpu_3",
  "kmem_cpu_4",
  "kmem_cpu_5",
  "kmem_cpu_6",
  "kmem_cpu_7",
};
void
kinit()
{
  // 修改：每个Kmem都需要初始化
  for(int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, kmem_lock_names[i]);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  
  r = (struct run*)pa;

  // 修改： 获得当前调用的CPU号
  int cpu = cpuid(); 
  // 这里，两个参考blog都使用push_off，目的应该是关闭中断
  // 使用的理由可能是book中断的说法： xv6的解决方式更加保守一些：只要CPU试图获取任何自旋锁，那么该CPU总是会关闭中断
  // 当着这里原本也没有使用，所以暂时维持原样

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  // 修改：根据CPU号，选择从哪个Kmem中变量freelist
  int cpu = cpuid();

  acquire(&kmem[cpu].lock);
  // r = kmem[cpu].freelist;
  // if(r)
  //   kmem[cpu].freelist = r->next;
  if (!kmem[cpu].freelist) {
    int steal_number = 10;  // 不确定可以偷多少页，这里随便给的一个值
    for(int i = 0;i < NCPU; i++) {
      if(i == cpu) continue;    // 不能偷自己的
      if (!kmem[i].freelist)  continue; // 偷的CPU一定要有空闲页

      acquire(&kmem[i].lock);
      struct run *rr = kmem[i].freelist;
      while(rr && steal_number) {
        kmem[i].freelist = rr->next;
        rr->next = kmem[cpu].freelist;
        kmem[cpu].freelist = rr;
        rr = kmem[i].freelist;
        steal_number--;
      }
      release(&kmem[i].lock);

      if(steal_number == 0) break;
    }
  }

  r = kmem[cpu].freelist;
  if(r)
    kmem[cpu].freelist = r->next;
  release(&kmem[cpu].lock);


  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
