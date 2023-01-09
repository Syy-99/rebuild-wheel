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
typedef char* va_list 
#define va_start(ap,arg) (ap=(va_list)&arg + sizeof(arg))
#define va_arg(ap,type) (*(t*)((ap +=sizeof(t)) - sizeof(t)))
#define va_end(ap) (ap=(va_list)0)

// arg1: 输出的文件
// arg2: 输出的格式控制字符串
// arg3: 需要被输出的可变参数列表
int vfprintf(FILE *stream, const char *format, va_list arglist) {
	int translating = 0;
	int ret = 0;
	const char * p = 0;

	for (p = format; *p != '\0'; p++) {
		switch (*p) {
			case '%':
				if (!translating)
					translating = 1;
				else { // 此即出现"%%"的格式串，实际上是要输出%
					if (fputc('%', stream) < 0)
							return EOF;
					++ret;
					translating = 0
				}
				break;
			case 'd':  // 遇到d，需要判断是实际要输出的字符，还是格式控制字符
				if (translating) { // %d
					char buf[16];
					translating = 0;

					itoa(va_arg(arglist,int), buf, 10);
					if (fputs(buf, stream) < 0)
						return EOF;
					ret += strlen(buf);
				} else {
					if (fputc('d', stream) < 0 )
						return EOF:
					ret++;
				}

				break;
			case 's':
				if (translating) {
					const char * str = va_arg(arglist, const char *);
					translating = 0;
					
					if (fputs(str, stream) < 0)
						return EOF;
					ret += strlen(str);
				} else {
					if (fputc('s', stream) < 0)
						return EOF;
					ret++
				}
				break;
			default:
				if (translating)
					translating = 0;		// 只支持%d %s
				if (fputc(*p, stream) < 0)
					return EOF;
				else
					ret++;
				break;
		}
	}
	return ret;
}
int printf(const char *format, ...) {
	va_list arglist;
	va_start(arglist, format);
	return vfprintf(stdout, format, arglist);
}

int fprintf(FILE *stream, const char *format, ...) {
	va_list arglist;
	va_start(arglist, format);
	return vfprintf(stream, format, arglist);
}
