#include "user/user.h"

int main(int argc, char *argv[]) {
	if (argc <= 1)          // 只有一个参数
		printf("no specified number of tricks\n");
	else if (argc > 2)
		printf("too many command argument\n");
	else            // 正常情况：只有一个参数
		sleep(atoi(argv[1]));           // 利用系统调用睡眠
	
	exit(0);
}
