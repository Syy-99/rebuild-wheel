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

2. 实现`sys_symlink()`(kernel/sysfile.c)

    ```c
    uint64
    sys_symlink(void)
    {
        struct inode *ip;

        // 1. 获得参数
        char target[MAXPATH], path[MAXPATH];
        if(argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0)
            return -1;

        // 2. 开始操作文件系统
        begin_op();

        ip = create(path, T_SYMLINK, 0, 0);   // 创建一个inode, 类型是T_SYMLINK
        if(ip == 0){
            end_op();
            return -1;
        }

        // inode的第一个直接数据块存储链接的文件名
        if(writei(ip, 0, (uint64)target, 0, strlen(target)) < 0) {
            end_op();
            return -1;
        }

        iunlockput(ip);

        end_op();
        return 0;
    }
    ```
3. 修改`sys_open()`(kernel/sysfile.c), 当遇到`T_SYMLINK`类型的inode时，需要读取它实际指向的文件inode

    ```c
      // 修改
    if(ip->type == T_SYMLINK && (omode & O_NOFOLLOW) == 0) {
        if((ip = follow_symlink(ip)) == 0) {
        // 此处不用调用iunlockput()释放锁,因为在follow_symlinktest()返回失败时ip的锁在函数内已经被释放
        end_op();
        return -1;
        }
    }

    if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
        if(f)
        fileclose(f);
        iunlockput(ip);
        end_op();
        return -1;
    }
    ```

    - `follow_symlink`:

        ```c
         struct inode* follow_symlink(struct inode* ip) {
            uint inums[10]; // 最多找10次
            int i, j;
            char target[MAXPATH];

            for(i = 0; i < 10; ++i) {
                inums[i] = ip->inum;

                // read the target path from symlink file
                if(readi(ip, 0, (uint64)target, 0, MAXPATH) <= 0) {
                    iunlockput(ip);
                    // printf("open_symlink: open symlink failed\n");
                    return 0;
                }
                iunlockput(ip);
                
                // get the inode of target path 
                if((ip = namei(target)) == 0) {
                    // printf("open_symlink: path \"%s\" is not exist\n", target);
                    return 0;
                }

                // 判断这个inode是否重复出现，如果重复出现则一定会有环
                for(j = 0; j <= i; ++j) {
                    if(ip->inum == inums[j]) {
                        // printf("open_symlink: links form a cycle\n");
                        return 0;
                    }
                }

                ilock(ip);

                if(ip->type != T_SYMLINK) {
                    return ip;
                }
            }

            iunlockput(ip);
            // printf("open_symlink: the depth of links reaches the limit\n");
            return 0;
        }
        ```

### 实验结果

```sh
init: starting sh
$ symlinktest
Start: test symlinks
open_symlink: path "/testsymlink/a" is not exist
open_symlink: links form a cycle
test symlinks: ok
Start: test concurrent symlinks
test concurrent symlinks: ok
$ QEMU: Terminated
syy@syyhost:~/xv6-labs-2020$ ./grade-lab-fs symlinktest 
make: 'kernel/kernel' is up to date.
== Test running symlinktest == (2.3s) 
== Test   symlinktest: symlinks == 
  symlinktest: symlinks: OK 
== Test   symlinktest: concurrent symlinks == 
  symlinktest: concurrent symlinks: OK 
```
