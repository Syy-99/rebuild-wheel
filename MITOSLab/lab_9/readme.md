# Lab: file system

## Large files

- In this assignment you'll **increase the maximum size of an xv6 file**. Currently xv6 files are limited to 268 blocks

- You'll change the xv6 file system code to support a **"doubly-indirect" block** in each inode, containing 256 addresses of singly-indirect blocks, each of which can contain up to 256 addresses of data blocks.

    - 修改后file最大block数 = 11(直接) + 256(单间接) + 256*256(双间接) = 65803


### 实验步骤

1. 根据实验要求，修改磁盘中的的inodex结构体和内存中的inode结构体

    ```c
    #define NDIRECT 11      //【修改】： 只有11个直接块了
    #define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)   // 【修改】

    struct dinode {
        short type;           // File type 文件类型
        short major;          // Major device number (T_DEVICE only)
        short minor;          // Minor device number (T_DEVICE only)
        short nlink;          // Number of links to inode in file system 有多个目录包含这一inode
        uint size;            // Size of file (bytes) 以字节为单位表示该文件有多大
        uint addrs[NDIRECT+ 2];   // Data block addresses  addrs数组则指向inode的数据块所在位置，每个数组条目都包含了一个数据块块号  【修改】
    };


    struct inode {
        uint dev;           // Device number
        uint inum;          // Inode number
        int ref;            // Reference count,  有多少指针指向该inoden内存副本
        struct sleeplock lock; // protects everything below here
        int valid;          // inode has been read from disk?

        // //以下为struct dinode的成员
        short type;         // copy of disk inode
        short major;
        short minor;
        short nlink;
        uint size;
        uint addrs[NDIRECT+2];  // 【修改】
    };

    ```


2. 修改`bmap()` 和 `itrunc()`

    ```c
    static uint
    bmap(struct inode *ip, uint bn)
    {
        uint addr, *a;
        struct buf *bp;

        // 判断是否为直接块
        if(bn < NDIRECT){
            if((addr = ip->addrs[bn]) == 0)
            // 该直接块还没有被分配，因此分配一个新数据块
            ip->addrs[bn] = addr = balloc(ip->dev);
            return addr;
        }
        bn -= NDIRECT;

        // 如果bn不是直接块，就查找间接块
        if(bn < NINDIRECT){
            // Load indirect block, allocating if necessary.
            if((addr = ip->addrs[NDIRECT]) == 0)
            // 一级间接映射块是空的，先分配一个一级间接块
            ip->addrs[NDIRECT] = addr = balloc(ip->dev);

            bp = bread(ip->dev, addr);

            a = (uint*)bp->data;
            if((addr = a[bn]) == 0){
            // 分配一个新数据块
            a[bn] = addr = balloc(ip->dev);
            // balloc只会将新的位图写入日志中，而一级间接块现在被更新
            // 所以要另外调用log_write将更新写入日志中
            log_write(bp);
            }
            brelse(bp);
            return addr;
        }

        bn -= NDIRECT;
        if(bn < NINDIRECT * NINDIRECT) { // doubly-indirect
            // Load indirect block, allocating if necessary.
            if((addr = ip->addrs[NDIRECT+1]) == 0)
            ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);

            bp = bread(ip->dev, addr);
            a = (uint*)bp->data;

            // 先获得第1级
            if((addr = a[bn/NINDIRECT]) == 0){
            a[bn/NINDIRECT] = addr = balloc(ip->dev);
            log_write(bp);
            }
            brelse(bp);

            // 再获得第2级
            bn %= NINDIRECT;
            bp = bread(ip->dev, addr);
            a = (uint*)bp->data;
            if((addr = a[bn]) == 0){
            a[bn] = addr = balloc(ip->dev);
            log_write(bp);
            }
            brelse(bp);
            return addr;
        }

        panic("bmap: out of range");
    }

    // Truncate inode (discard contents).
    // Caller must hold ip->lock.
    // 释放一个给定inode的所有数据块，并且将inode长度截断为0
    void
    itrunc(struct inode *ip)
    {
        int i, j;
        struct buf *bp;
        uint *a;

        // 释放所有直接块
        for(i = 0; i < NDIRECT; i++){
            if(ip->addrs[i]){
            bfree(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
            }
        }

        // 释放所有间接块
        if(ip->addrs[NDIRECT]){
            bp = bread(ip->dev, ip->addrs[NDIRECT]);
            a = (uint*)bp->data;
            for(j = 0; j < NINDIRECT; j++){
            if(a[j])
                bfree(ip->dev, a[j]);
            }
            brelse(bp);
            // 释放间接块
            bfree(ip->dev, ip->addrs[NDIRECT]);
            ip->addrs[NDIRECT] = 0;
        }

        // 释放双间接块
        if (ip->addrs[NINDIRECT+1]) {
            bp = bread(ip->dev, ip->addrs[NDIRECT+1]);
            a = (uint*)bp->data;
            for (j = 0; j < NINDIRECT; j++) {
            struct buf *bp2;
            if (a[j]) {
                bp2 = bread(ip->dev, a[j]);
                uint *a2 = (uint*)bp2->data;
                for (int k = 0; k < NINDIRECT; k++) {
                    if (a2[k])
                    bfree(ip->dev,a2[k]);
                }
                brelse(bp2);
                bfree(ip->dev, a[j]);
            }
            }
            brelse(bp);
            bfree(ip->dev, ip->addrs[NDIRECT + 1]);
            ip->addrs[NDIRECT + 1] = 0;
        }

        ip->size = 0;
        iupdate(ip);
    }
    ```

    -  只要将原始bmap和itrunc弄懂，修改起来很简单

### 实验结果

```sh
$ bigfile
..................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................
wrote 65803 blocks
bigfile done; ok

$ usertests
#...
test bigdir: OK
ALL TESTS PASSED
```


## Symbolic links

- In this exercise you will add symbolic links to xv6

- You will implement the `symlink(char *target, char *path)` system call, which creates a new symbolic link at path that refers to file named by target

### 实验步骤

1. 增加一个新的系统调用 

    - 具体流程略