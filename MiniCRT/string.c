#include "minicrt.h"

// 将一个数字转换为对应的radix进制数的表示形式，保存在str中
char* itoa(int n, char* str, int radix)
{
    char digit[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char* p = str;
    char* head = str;

    if(!p || radix < 2 || radix > 36) //radix代表是几进制
        return p;
    if(radix != 10 && n < 0)
        return p;

    if (n == 0) //如果要转换的数字n为0，则直接在输出字符串中直接输出
    {
        *p++ = '0';
        *p = 0;
        return p;
    }
    if (radix == 10 && n < 0) //如果是10进制，且为负数，则先添加负号，然后转正留待后续处理
    {
        *p++ = '-';
        n = -n;
    }

    while (n)
    {
        *p++ = digit[n % radix];
        n /= radix;
    }
    *p = 0; //数字转换完了，末尾添加0

    //上面的数字字符串是倒序的，这里将数字字符串倒过来
    for (--p; head<p; ++head, --p)
    {
        char temp = *head;
        *head = *p;
        *p = temp;
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
