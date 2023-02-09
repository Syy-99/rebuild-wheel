#include "user/user.h"
int main() {
	int fd1[2], fd2[2];
	pipe(fd1);
	pipe(fd2);		// 双向传递信息就需要两个管道
	
	if (fork() != 0) { // 在父进程中
		write(fd1[1], "!", 1);
		
		char buf;
		read(fd2[0], &buf, 1);
		printf("%d: received pong\n", getpid());	

		// 关闭无用文件描述符引用
		close(fd1[0]);
		close(fd2[1]);
	} else if (fork() == 0) {
		char buf;
		read(fd1[0], &buf, 1);			// 因为父进程拥有fd[1], 因此当还没有数据时，read()会阻塞
		printf("%d: received ping\n", getpid());

		write(fd2[1], "!", 1);

		// 关闭无用文件描述符
		close(fd1[1]);
		close(fd2[0]);
	} 
	// else {
	// 	printf("fork() error %d\n", getpid());
	// }
	
	//程序结束时会自动关闭该程序打开的文件描述符
	exit(0);
}
