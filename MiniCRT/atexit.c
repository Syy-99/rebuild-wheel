#include "minicrt.h"

typedef struct _func_node {
	atexit_func_t func;
	void *arg;
	int is_cxa;
	struct _func_node *next;
}func_node;

static func_node * atexit_list = 0;

int register_atexti(atexit_func_t func, void *arg, int is_cxa) {
	func_node *node;
	if (!func)
		return -1;

	node = (func_node*)malloc(sizeof(func_node));
	if (node == -1)
		return -1;

	node->func = func;
	node->arg = arg;
	node->is_cxa = is_cxa;
	// 头插法
	node->next = atexit_list;
	atexit_list = node;

	return 0;
}

int atexit(atexit_func_t func) {
	return register_atexti(func, 0, 0);
}

//GCC实现全局对象的析构使用__cxa_atexit()，该函数必须有一个void*指针作为参数，并且执行时需要将该参数传入
typedef void (*cxa_func_t)(void*);
int __cxa_atexit(cxa_func_t func, void *arg, void *unused) {
	return register_atexti((atexit_func_t)func, arg,1);
}

void mini_crt_call_exit_routine() {
	func_node *p = atexit_list;

	for(; p != 0; p = p->next) {
		if (p->is_cxa)
			((cxa_func_t)p->func_node)(p->arg);
		else
			p->func();
		free(p);
	}
	atexit_list = 0;
}
	
