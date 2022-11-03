#include <stdio.h>
#include <sys/types.h>		// 提供类型pid_t和size_t的定义
#include <sys/socket.h>		// 提供套接字相关函数
#include <netinet/in.h>
#include <arpa/inet.h>		// 提供网络字节序和主机字节序转换的相关函数
#include <unistd.h>			// 提供基于套接字描述符的write,close函数

// 客户端程序
int main(int argc, char *argv[])
{
    int sockfd;
    int len;
    struct sockaddr_in address;     // 用来记录sockfd绑定的地址信息
    int result;
    char ch = 'A';

    sockfd = socket(AF_INET, SOCK_STREAM, 0);   // 创建套接字
    // 设置地址信息
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");   // 套接字sockfd请求连接的主机是此台主机
    address.sin_port = htons(9734);     // 套接字sockfd访问的主机端口是9734
    len = sizeof(address);
    result = connect(sockfd, (struct sockaddr *)&address, len);     // 客户端请求连接127.0.0.1:9734

    if (result == -1)
    {
        perror("oops: client1");
        exit(1);
    }
    write(sockfd, &ch, 1);      // 传递信息'A'
    read(sockfd, &ch, 1);       // 读信息
    printf("char from server = %c\n", ch);
    close(sockfd);      // 关闭套接字
    exit(0);
}
