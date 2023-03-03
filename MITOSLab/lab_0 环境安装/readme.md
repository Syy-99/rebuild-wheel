## 环境搭建

VMware + Ubuntu-22.04.01服务器版 + VScode远程连接虚拟机

### VMware安装

略

### Ubuntu镜像文件下载及使用

略

### VScode远程连接

- [VsCode配置ssh免密远程登录](https://blog.csdn.net/qq_44571245/article/details/123031276)

## 环境安装

[MIT6.S081 Ubuntu22.04 WSL2实验环境配置](https://zhuanlan.zhihu.com/p/537461426)

1. 编译工具安装

   ```sh
   sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu 
   
   # 测试是否安装成功
   1. qemu-system-riscv64 --version
   2. riscv64-linux-gnu-gcc --version
   ```

2. 下载课程所需的xv6内核代码

   ```sh
   git clone git://g.csail.mit.edu/xv6-labs-2020
   ```

   -  进而该文件然后切换分支

     ```sh
     git checkout util
     ```

3. 编译并运行xv6

   ```sh
   # 在xv6文件中
   make qemu
   ....
   hart 2 starting
   hart 1 starting
   init: starting sh
   $ 		# 说明成功
   ```

   - 如果Ubuntu的版本为22.04，编译会不成功一直卡在最后一步。这是因为**Ubuntu系统的版本过高，默认安装的qemu模拟器与xv6内核不兼容，导致编译不成功**，根据官网提示，需要qemu的版本为5.1。

     ```sh
     # 1. 卸载qemu
     sudo apt-get remove qemu-system-misc
     # 2. 安装旧版本
     sudo apt-get install qemu-system-misc=1:4.2-3ubuntu6	# 但是ubuntu22.04已经找不到该版本,需要手动安装
     # 2.1
     wget https://download.qemu.org/qemu-5.1.0.tar.xz
     tar xf qemu-5.1.0.tar.xz
     
     cd qemu-5.1.0
     sudo apt-get install libglib2.0-dev libpixman-1-dev
     ./configure --disable-kvm --disable-werror --prefix=/usr/local --target-list="riscv64-softmmu"
     
     make 
     sudo make install
     # 然后再重新执行 make qemu
     ```
     
   - 退出qemu: `Ctrl-a x`

4. Python安装并加入环境变量：

   > 为了测试程序分数

   ```sh
   # 安装python3后，还需要建立python软链接给测试程序用
   syy@syyhost:~/xv6-labs-2020$ whereis python3
   python3: /usr/bin/python3 /usr/lib/python3 /etc/python3 /usr/share/python3 /usr/share/man/man1/python3.1.gz
   syy@syyhost:~/xv6-labs-2020$ sudo ln -s /usr/bin/python3 /usr/bin/python
   ```

## 利用gdb调试程序

[如何在QEMU中使用gdb](https://gwzlchn.github.io/202106/6-s081-lab0/)

