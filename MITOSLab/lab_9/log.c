#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;      //表示block中有多少块要写入磁盘
  int block[LOGSIZE];   // block中记录了log block要写入的磁盘位置(log block的数量就是30=cache的数量)
};

struct log {
  struct spinlock lock;
  int start;    // 记录日志区域开始的磁盘位置
  int size;     // size是日志区域的总块数
  int outstanding; // how many FS sys calls are executing.
                    // 只有在outstanding=0时，才可提交log
  int committing;  // in commit(), please wait.  表示日志系统是否正在加检查点
  int dev;        // 磁盘设备的标识
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

// 从超级块上，读取关于日志系统的信息，包括日志区域的起始位置，日志区域的大小
void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart; // 记录磁盘上log区块开始的地址
  log.size = sb->nlog;    // 记录磁盘上有多少个log block
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
// 将每个logged blocks写到目的磁盘位置上,然后bunpin其在内存中的相应缓存块，这样Buffer Cache可以回收它。
// 本次事务就成功更新到磁盘
static void
install_trans(int recovering)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);  // write dst to disk

    if(recovering == 0)
      bunpin(dbuf);   // 在log_write中bpin了相应的缓存块，这里bunpin它
                      // 因此在这之后，这一缓存块可以被Buffer Cache回收
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
// 将更新后的logheader写回磁盘上，完成日志的提交
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);

  brelse(buf);
}


//读出磁盘上的logheader，如果计数n=0，代表日志系统不需要进行恢复，因此跳过recovery继续启动；
// 如果计数n不等于0，代表有需要重放的日志，因此我们回到数据日志四部曲中的加检查点，再次调用install_trans重新执行加检查点过程。
// 最后在函数退出点，清除旧日志
static void
recover_from_log(void)
{
  read_head();
  
  install_trans(1); // if committed, copy from log to disk
  
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){    // xv6的每次FS系统调用最多只会提交数量为MAXOPBLOCKS的日志块
      // this op might exhaust log space; wait for commit.
       // 如果当前日志区域没有足够空间，先挂起
      sleep(&log, &log.lock);
    } else {
      // 受理该FS系统调用请求
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;

  // 没有事务能够在还有FS调用的情况下就被提交
  // 如果发生了，就是panic
  if(log.committing)
    panic("log.committing");

  if(log.outstanding == 0){   // 当前没有FS sys calls，可以开始事务提交
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    // 如果当前还有其它FS调用正在执行，那么我们先不急着提交
    // 而是留给最后一个FS调用负责提交所有事务，这是group commit

    // 此外，可能有其它FS调用在begin_op时因空间不足而被挂起
    // 现在可能有空间可用，唤醒一个被挂起的FS调用
    wakeup(&log);
  }
  release(&log.lock);

  // 注意，这里是没有锁保护的
  // 所以可能会切换其他进程，但是此时do_commit和log.committing可能等于1
  // 所以，有些操作需要检查log.committing的值
  if(do_commit){
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire(&log.lock);
    log.committing = 0;   // 表示日志系统提交完成并处于空闲
    wakeup(&log);
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
// 将所有位于内存中的，更新后的缓存块，按顺序写入到磁盘的日志区域上
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    // 从磁盘上按顺序读出日志区域的磁盘块，额外加1是跳过logheader
    struct buf *to = bread(log.dev, log.start+tail+1); // log block
    // 从buffer cache中读取缓存对应磁盘号的cache
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block

    // 将缓存写入对应的日志区域的磁盘块上
    memmove(to->data, from->data, BSIZE);   // 这里还在内存
    bwrite(to);  // write the log

    // 因为已经写到日志里了，所以可以释放
    brelse(from);  // 放更新链表
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log
    write_head();    // Write header to disk -- the real commit
    install_trans(0); // Now install writes to home locations
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log
                      // 将磁盘中的logheader的n更新为0
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  // 检查当前已经写入log header的log blocks数量，上限为LOGSIZE
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");



  acquire(&log.lock);
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorbtion
      // 如果之前已经在log中有提交对同一个块b的更新
      // 就不用分配新的logged block，直接修改上次的即可
      break;
  }
  // 如果之前没有提交过块b的更新，那么i=log.lh.n
  // 然后紧接着上次写日志的地方，往logheader的block[n]写入块b的目标磁盘位置(顺序写)
  log.lh.block[i] = b->blockno;

  if (i == log.lh.n) {  // Add new block to log?
    // 为了不违反原子性，cache块不应该被逐出，因为逐出会马上被写入到磁盘上
    // 所以调用bpin增加其引用计数refcnt，这样就不会被逐出
    bpin(b); 
    log.lh.n++;
  }
  release(&log.lock);
}

