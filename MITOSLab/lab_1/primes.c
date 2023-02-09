#include "user/user.h"
// 每个子进程的逻辑都是一样的，所以抽象成一个函数
void sieve(int fd[2]) {
	int base;
	if (read(fd[0], &base, sizeof(base)) > 0){		// 第一个传递的数一定是素数
		printf("prime %d\n", base);

		int right_pipe[2];
		pipe(right_pipe);

		if (fork() > 0) {
			close(right_pipe[1]);
			close(fd[0]);
			sieve(right_pipe);
			
		} else {	// 该进程基于base进行筛选
			int num;
			while (read(fd[0], &num, sizeof(num)))
			{
				if (num % base)
					write(right_pipe[1], &num, sizeof(num));
			}
			
			close(right_pipe[1]);
			close(right_pipe[0]);

			wait(0);
		}
	} 
	exit(0);
}

int main() {
  int fd[2];
  pipe(fd);
	if(fork() > 0) {
 		for (int i = 2; i <=35; i++)
			write(fd[1], &i, sizeof(i));

		close(fd[1]);			// 输入完后要关闭，否则读端会始终阻塞
		close(fd[0]);			// 减少资源占用

		wait(0);	// 实验要求：等待子进程结束，父进程才能结束
	} else {
		close(fd[1]);
		sieve(fd);
	}
	exit(0);
}
