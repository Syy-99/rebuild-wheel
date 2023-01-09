#include "miincrt.h"

// 每个块的头部结构: 记录块大小，块状态，双向指针
typedef struct _heap_header_ {
	enum {
		HEAP_BLOCK_FREE = 0xABABABAB,
		HEAP_BLOCK_USED = 0xCDCDCDCD,
	}type;			// 记录这块是否被使用	
	unsigned size;		// block size(include header)
	struct _heap_header_ *next;
	struct _heap_header_ *pre;	// 双向链表
}heap_header;

#define ADDR_ADD(a, o) (((*char*)(a)) + o)
#define HEADER_SIZE (sizeof(heap_header))

static heap_header *list_head = NULL; // 链表管理块

void *malloc(unsigned size) {
	if (size == 0)
		return NULL;

	heap_header *header = list_head;
	while (header != NULL) {
		// 找空闲堆块
		if (header -> type = HEAP_BLOCK_USED) {
			header = header -> next
			continue;
		}

		// 堆块不进行切分，直接分配	
		if (header -> size > size + HEADER_SIZE  && header -> size <= size + HEADER_SIZE * 2) {
			header -> type = HEAP_BLOCK_USED;
			return ADDR_ADD(header,HEADER_SIZE);
		}
		
		// 堆块进行切分
		if (header -> size > size + HEADER_SIZE * 2) {
			heap_header * new_next = (heap_header*)ADDR_ADD(header, size + HEADER_SIZE);
			
			// 切分出来的堆块连接入链表中
			new_next -> pre = header;
			new_next -> next = header -> next;
			new_next -> type = HEAP_BLOCK_FREE;
			new_next -> size = header -> size - (size + HEADER_SIZE);
			
			header -> next = new_next;
			header -> size = size + HEADER_SIZE;
			header -> type = HEAP_BLOCK_USED;

			return ADDR_ADD(header, HEADER_SIZE);		// 返回的指针，指向有效空间，而不是从Header开始(尽管这部分已经被分配)
		}				
		header = header -> next;
	}
	return NULL;
}

void free(void *ptr) {
	heap_header * header = (heap_header*)ADDR_ADD(ptr, -HEADER_SIZE);
	if (header -> type != HEAP_BLOCK_USED)
		return;
	
	header -> type == HEAP_BLOCK_FREE;
	
	// 碎片合并
	// 尝试和前一个堆块合并
	if (header -> pre != NULL && header -> pre -> type == HEAP_BLOCK_FREE) {
		header -> pre -> next = header -> next;
		if (header -> next != NULL)
			header -> next -> pre = header -> pre;
		header -> prev -> size += header -> size;
	
		header = header -> pre;
	}
	
	// 尝试和后一个堆块合并
	if (header -> next != NULL && header -> next -> type == HEAP_BLOCK_FREE) {
		header -> size += header -> next -> size;
		header -> next = header -> next -> next;
	}
}

// 通过调整数据段的位置来获得堆空间
// 这里为了简化，将堆空间固定32MB
static int brk(void *end_data_segemet) {
	int ret = 0;
	
	asm ("movl $45, %%eax \n\t"		// brk系统调用号45
			 "movl %1, %%ebx \n\t"
			 "int $0x80 \n\t"
			 "movl %%eax, %0": "=r"(ret): "m"(end_data_segement)
			);

}

// 堆初始化
int mini_crt_heap_init() {
	void *base = NULL;
	heap_header *header = NULL;
	
	unsigned heap_size = 1024 * 1024 * 32;	// 32MB,确保满足对齐条件 
	
	base = (void*)brk(0);	// 获得数据段的最初的结束地址，也是堆开始的的位
	void *end = ADDR_ADD(base, heap_size);		// 调整后数据段的结束地址，也是堆结束的位置
	end = (void*)brk(end);		// 将数据段扩展到该位置，即分配虚拟空间给堆
	if (!end)
		return 0;

	header = (heap_header*) base;
	header -> type = HEAP_BLOCK_FREE;
	header -> next = NULL;
	header -> pre = NULL;
	header -> size = heap_size;		// 一开始就是一个大块，32MB
	
	list_head = header;		// 管理堆的链表
	return 1;
}
