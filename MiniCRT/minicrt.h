#ifndef __MINI_CRT_H__
#define __MINI_CRT_H__

#ifdef __cplusplus  // gcc编译器会根据编译的类型自动判断是否定义该宏
extern "C" {
#endif

//malloc 
#ifndef NULL
#define NULL (0)
#endif

void free(void* ptr);
void* malloc(unsigned size);
static int brk(void *end_data_segment);
int mini_crt_heap_init();

// 字符串
char* itoa(int n,char* str,int radix);
int strcmp(const char* src, const char* dst);
char* strcpy(char* dest, const char* src);
unsigned strlen(const char* str);

// 文件与IO	
// 注意，这里FILE实际上就是文件描述符
typedef int FILE;

#define EOF	(-1)
#define stdin	((FILE*)0)
#define stdout	((FILE*)1)
#define stderr	((FILE*)2)

int mini_crt_io_init();
FILE* fopen(const char *filename,const char* mode);
int fread(void* buffer,int size,int count,FILE* stream);
int fwrite(const void* buffer,int size,int count,FILE* stream);
int fclose(FILE *fp);
int fseek(FILE* fp,int offset,int set);

//printf
int fputc(int c,FILE* stream);
int fputs(const char* str,FILE *stream);
int printf(const char *format,...);
int fprintf(FILE* stream,const char *format,...);

// 下面的函数是针对C++程序的全局构造和全局析构
//internal 
void do_global_ctors();
void mini_crt_call_exit_routine();

//atexit
typedef void(*atexit_func_t )( void );
int atexit(atexit_func_t func);

#ifdef __cplusplus
}
#endif


#endif // end __MINI_CRT_H__
