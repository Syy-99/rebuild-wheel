struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;     
  uint blockno;

  struct sleeplock lock;
  uint refcnt;

  struct buf *prev; // LRU cache list
  struct buf *next;

  uint lastuse;   // 记录该buf被使用的ticks
  uchar data[BSIZE];
};

