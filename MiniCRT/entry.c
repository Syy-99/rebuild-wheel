#include "minicrt.h"

extern int main(int argc, char** argv);
void exit(int);

static void crt_fatal_error(const char *msg) {
	// printf("fatal error: %s", msg);
	exit(1);
}

// 入口函数负责三部分工作：
// 1. 初始化程序运行环境: main的参数；初始化运行时库；堆；IO等
// 2. 调用main
// 3. 清理资源
void mini_crt_entry() {
	// 根据栈的结构，获得main的参数
	char *ebp_reg = 0;
	asm("movl %%epb, %0" : "=r"(ebp_reg));
	int argc;
	char **argv;
 	argc = *(int*)(ebp_reg + 4);
  argv = (char**)(ebp_reg + 8);
		
  // 堆初始化
	if (!mini_crt_heap_init())
		crt_fatal_error("heap initialize failed");
	
	// io初始化
	if (!mini_crt_io_init())
		crt_fatal_error("io initialize failed");

	// 调用main
	int ret = main(argc, argv);

	// 清理资源
	exit(ret);
	
}

void exit (int exit_code) {
	// mini_crt_call_exit_routine();
	asm ("movl %0, %%ebx \n\t"
			 "movl $1, %%eax \n\t"
			 "int $0x80 \n\t"
		 	 "hlt \n\t" ::"m"(exit_code)
			);
}
