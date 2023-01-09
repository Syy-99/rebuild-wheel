#include "minicrt.h"

// 将一个数字转换为对应的radix进制数的表示形式，保存在str中
char* itoa(int n,char* str,int radix)
{
	char digit[]	="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	
	// 无法进行进制转换的情况
  if(!str || radix < 2 || radix > 36)
		return str;	

	unsigned int  = value;
	// 对于转换目标是10进制，因为10进制的正负体现在前面符号上，所以需要特殊处理
	int neg = 1;
	if (radix == 10 && n < 0) {
			neg = -neg;
			n = -n;
	} else {
		value = (unsigned int)n;
	}
	// 例如，假设int n n=0xffffffff, 那么实际上n=-1
 	// 如果radix=2, 只要按照0xfffffffff转换即可
	// 我们使用10进制转n进制的辗转相除法，从digit数组中获得表示，所以该方法获得的索引不能是负的，因此对于n<0,一定要转换为n>0
	// 当是，当-1转换为1时，对1进行辗转相处的二进制变成了01, 所以我们需要保证-1的二进制表示，因此需要使用unsigned int来转换原先的n的类型
	// 如果radix=10, 因为n此时的数值就是十进制的表示，所以我们只需要将n转换为字符串表示形式即可
	// 首先‘-’是无法计算得到的，只能手动添加，而索引又不能是负的，所以需要将n转换为-n即可正常处理

	// 开始辗转相除
	char *p = str;
	while(n) {
		*p++ = digit[n % radix];
		n /= radix;	
	}
	if (neg == -1)
		*p++ = '-';
	p = '\0';

	// 注意需要翻转才是最终结果
	char *p2 = str;
	p--;
	for(; p2 < p; ++p2, --p) {
		char temp = *p2;
		*p2 = *p;
		*p = temp;
	}

	return str;

}

	return str;
}

int strcmp(const char* src, const char* dst)
{
	int ret	= 0;
	// ASCII总共有256个，需要8个字节表示
	// 如果是char类型，在转换int时，大于127的会变成负数，导致比较错误
	// 因此这里将它们全部转换unsigned char 类型
	unsigned char* p1	= (unsigned char*)src;
	unsigned char* p2	= (unsigned char*)dst;

	// 注意，这个函数没有处理src，dst是NULL的情况；
	// while保证循环的条件是*p1==*p2,此时如果*p1==0，则说明两者都比较完了，可以直接根据ret确定最终结果
	while(!(ret = *p1 - *p2)&&*p2)	
		++p1,++p2;
	
	if (ret < 0)
	{
		ret	= -1;
	}
	else if (ret > 0)
	{
		ret	= 1;
	}

	return (ret);
}

char * strcpy(char* dest, const char* src)
{
	char* ret	= dest;
	while(*src)
	{
		*dest++ = *src++;
	}
	*dest	= '\0'; // 字符串数组以\0结尾
	return ret;

}

unsigned strlen(const char* str)
{
	int cnt	= 0;
	if(!str)	// 处理str=null的情况
		return 0;
	for (;*str != '\0';++str)
	{
		++cnt;
	}

	return cnt;
}
