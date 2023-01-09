#include "minicrt"

int fputc(int c, FILE *stream) {
	if (fwrite(&c,1,1,stream) != 1)
		return EOF;
	else
		return c;
}

int fputs(const char *str, FILE *stream) {
	int len = strlen(str);
	if (fwrite(str, 1, len, stream) != 1) 
		return EOF;
	else 	
		return len; 
}

// 处理c语言可变参数
#define va_list char*
#define var_start(ap,arg) (ap=(va_list)&arg + sizeof(arg))
#define var_arg(ap,type) (*(t*)((ap +=sizeof(t)) - sizeof(t)))
#define var_end(ap) (ap=(va_list)0)

// arg1: 输出的文件
// arg2: 输出的格式控制字符串
// arg3: 需要被输出的可变参数列表
int vprintf(FILE *stream, const char *format, va_list arglist) {
	int translating = 0;
	int ret = 0;
	const char * p = 0;

	for (p = for
}
int printf(const char *format, ...) {

}
