// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"


#define NBUFMAP_BUCKET 13
#define BUFMAP_HASH(dev, blockno) ((((dev)<<27)|(blockno))%NBUFMAP_BUCKET)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct buf bufmap[NBUFMAP_BUCKET];    // 根据访问的设备号和块号，确定它会被缓存的桶中
  struct spinlock bufmap_locks[NBUFMAP_BUCKET];
} bcache;

void
binit(void)
{
  // 初始化桶和桶锁
  for(int i=0;i<NBUFMAP_BUCKET;i++) {
    initlock(&bcache.bufmap_locks[i], "bcache_bufmap");
    bcache.bufmap[i].next = 0;
  }

  // 初始化所有可用的buf, 并且默认将其全部分配到桶0中
  for(int i=0;i<NBUF;i++){
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->lastuse = 0;
    b->refcnt = 0;

    // put all the buffers into bufmap[0]
    b->next = bcache.bufmap[0].next;
    bcache.bufmap[0].next = b;
  }
}

// 获得某个设备对应块的buf
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint key = BUFMAP_HASH(dev, blockno);

  // 1. 根据哈希函数，到对应桶中判断是否有它的块的缓存
  acquire(&bcache.bufmap_locks[key]);   // 需要加锁

  for(b = bcache.bufmap[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      acquiresleep(&b->lock);
      return b;
    }
  } 
  release(&bcache.bufmap_locks[key]); 
  // 这里必须释放，否则会出现两个请求出现环路死锁
  // 但是如何解决两个进程访问同一个不存在的block问题呢？ (导致一个block可能被缓存两次)
  // 目前想法是：再检查一次

  // 到这里，说明对应桶中没有它的块缓存，则为其添加缓存，并加入对应桶中
  acquire(&bcache.lock);    // 避免选择的buf又被其他线程选中

  struct buf *before_least = 0; 
  uint holding_bucket = -1;
  for(int i = 0; i < NBUFMAP_BUCKET; i++){
 
    acquire(&bcache.bufmap_locks[i]);

    for(b = &bcache.bufmap[i]; b->next; b = b->next) {
      if(i == key && b->dev == dev && b->blockno == blockno){
        b->refcnt++;
        release(&bcache.bufmap_locks[key]);
        release(&bcache.lock);
        acquiresleep(&b->lock);
        return b;
      }
        // 找某个桶中的最久未使用的buf， 并且保存它的前一个结点指针
      if(b->next->refcnt == 0 && (!before_least || b->next->lastuse < before_least->next->lastuse)) {
          before_least = b;
          holding_bucket = i;
        }
    }

    release(&bcache.bufmap_locks[i]);
  }

  if(!before_least) {
    panic("bget: no buffers");
  }
  b = before_least->next;

  if(holding_bucket != key) {
    // remove the buf from it's original bucket
    before_least->next = b->next;

    acquire(&bcache.bufmap_locks[key]);
    b->next = bcache.bufmap[key].next;
    bcache.bufmap[key].next = b;
  }

  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  if(holding_bucket != key)
    release(&bcache.bufmap_locks[key]);
  release(&bcache.lock);

  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->lastuse = ticks;
  }
  release(&bcache.bufmap_locks[key]);
}

void
bpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);
}

void
bunpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);
}

