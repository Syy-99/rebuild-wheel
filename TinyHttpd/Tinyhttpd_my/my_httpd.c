// @brief: TinnyHttp 简单Web服务器实现
// @data: 2022.11.10
// @birth:

#include<stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include<string.h>	// 提供字符串处理函数
#include <pthread.h>	// 提供线程函数
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>   
#include <sys/wait.h>   // waitpid()
#include <sys/socket.h>	// 提供套接字相关处理函数
#include <arpa/inet.h>	// 提供主机地址和网络地址转换函数


#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

#define ISspace(x) isspace((int)(x))

void *AcceptRequest(void *arg);
int InitialServerListenSock(u_short listen_port);
int get_line(int sock, char *buf, int size);

void BadRequest(int client_sock);
void UnimplementedMethod(int client_sock);
void CannotExecute(int client_sock);
void NotFound(int client_sock);
void ErrorHandling(const char *msg);

void ResponseHeaders(int client_sock, const char *filename);
void SendFileContent(int client_sock, FILE *resource);

void ServeFile(int client_sock, const char *filename);
void ExecuteCgi(int client_sock, const char *path, const char *method, const char *query_string);

// @brief: 每个线程的函数，用来进行数据传输
// @param 1: 指向保存有服务器端进行连接的套接字文件描述符的地址的指针
// @return: void* 
void *AcceptRequest(void *arg)
{
	int client_sock = *((int*)arg); // 获得客户端套接字
    int cgi = 0;
	
	// 读取客户端发送的HTTP请求报文
    // 此时，req_mesg的格式应该类似:GET /index.html HTTP/1.1\r\n...
	char req_mesg[1024];
    int j = 0;    // 用来对req_mesg进行遍历去值的索引
    get_line(client_sock, req_mesg, sizeof(req_mesg));

	char method[255];   // 保存方法： GET or POST
    int i = 0;
    // 读取Http请求的方法
    while (!ISspace(req_mesg[i]) && i < strlen(req_mesg))    // 以空格结尾
    {
        method[i] = req_mesg[i];
        i++;
    }
    method[i] = '\0';
    j=i;
	// 判断method的合法性
    if (strcasecmp(method, "POST") == 0)	// 如果是POST方法, 则开启CGI
        cgi = 1;
	else if (strcasecmp(method,"get")) {	// 如果不是POST 和 get方法
  	  	UnimplementedMethod(client_sock);
		return NULL;
	}

	// 获取请求的服务端文件
	char file_name[255];
    //跳过多余的空格
    while (ISspace(req_mesg[j]))
        j++;
    i = 0;
    while (!ISspace(req_mesg[j]) && i < strlen(req_mesg))
    {
        file_name[i] = req_mesg[j];
        i++; j++;
    }
    file_name[i] = '\0';

	char *query_string;	// 记录file_name ? 的指针
	if (strcasecmp(method, "GET") == 0) {
		query_string = file_name;
		 // 检查query_string中是否存在 ? 
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;

        // Get中,?后面是参数
        if (*query_string == '?')
        {
            cgi = 1;        // 开启cgi
            *query_string = '\0';   // 此时file_name只保存?前的文件,
            query_string++;         // query_string指向?后的地址
        }
	}

	char path[1024];	// 保存实际的文件路径
	sprintf(path, "htdocs%s", file_name);	  //从TinyHTTPd项目的htdocs目录下提供文件(不会包括?后面的参数)
	
    if (path[strlen(path) - 1] == '/')  // 如果是目录类型，
		strcat(path, "index.html"); // 则默认访问index.html

    struct stat st;
	if (stat(path, &st) == -1) { // 如果找不到文件
         // 读取且销毁HTTP请求消息的请求行+消息头
        int numchars = 0;
        do {
            numchars = get_line(client_sock, req_mesg, sizeof(req_mesg));
        }while((numchars > 0) && strcmp("\n", req_mesg));
		NotFound(client_sock);  // 向客户返回404信息
    } else {
		if ((st.st_mode & S_IFMT) == S_IFDIR)   // 如果该文件是个目录，则默认使用它下面的index.html文件
			strcat(path, "/index.html");
		// 如果文件有可执行权限，开启 cgi
		if ((st.st_mode & S_IXUSR) ||
			(st.st_mode & S_IXGRP) ||
			(st.st_mode & S_IXOTH)    )
		cgi = 1;

		if (!cgi)   // 如果是静态页面
			ServeFile(client_sock, path);
		else       // 如果是动态页面请求
			ExecuteCgi(client_sock, path, method, query_string);
	}
	close(client_sock);
    return NULL;    // 修改1： 
}

// @brief: 发送正确响应信息的响应头
// @param 1: 客户端套接字
// @param 2: 具体文件路径
void ResponseHeaders(int client_sock, const char *filename)
{
    char buf[1024];

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
	strcat(buf, SERVER_STRING);
	strcat(buf, "Content-Type: text/html\r\n\r\n");    
	send(client_sock, buf, strlen(buf), 0);
}
// @brief: 发送正确响应信息的内容
// @param 1: 客户端套接字
// @param 2: 具体文件路径
void SendFileContent(int client_sock, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);     // 按行读取
    while (!feof(resource))
    {
        send(client_sock, buf, strlen(buf), 0);  
        fgets(buf, sizeof(buf), resource);
    }
}
// @brief: 返回静态页面
// @param 1: 客户端套接字的文件描述符
// @param 2: 具体文件路径
// @return: void
void ServeFile(int client_sock, const char *filename)
{
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    // 处理消息头
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client_sock, buf, sizeof(buf));
    // 修改：不需要处理消息头

    FILE *resource = fopen(filename, "r");    // 以只读的方式打开文件
    if (resource == NULL)
        NotFound(client_sock);
    else
    {
        ResponseHeaders(client_sock, filename);
        SendFileContent(client_sock, resource);
    }
    fclose(resource);
}
// @brief: 将请求传递给cgi程序处理
// @param 1: 客户端套接字的文件描述符
// @param 2: 文件路径
// @param 3: HTTP请求方法
// @param 4: 指向?的指针
void ExecuteCgi(int client_sock, const char *path, const char *method, const char *query_string) {
	char buf[1024];

    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A'; 
	buf[1] = '\0';

    if (strcasecmp(method, "POST") == 0) {
		
        // 如果是POST请求，就需要得到 Content-Length
        // Content-Length 字符串长度为15
        // 从 17 位开始是具体的长度信息
        numchars = get_line(client_sock, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));
            numchars = get_line(client_sock, buf, sizeof(buf));  // 循环读取消息头的字段消息，直到找到content-Length
        }
        if (content_length == -1) { 
            BadRequest(client_sock);    ////请求的页面数据为空，没有数据，就是我们打开网页经常出现空白页面
            return;
        }
    }

    // 建立管道
    if (pipe(cgi_output) < 0) {
        CannotExecute(client_sock);
        return;
    }
    if (pipe(cgi_input) < 0) {
        CannotExecute(client_sock);
        return;
    }
    // 创建子进程
    if ( (pid = fork()) < 0 ) {
        CannotExecute(client_sock);
        return;
    }
    // 状态行
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client_sock, buf, strlen(buf), 0);

    if (pid == 0)  /* child: CGI script */
    {   // / 子进程用于执行 cgi
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        // 这部分操作可以参考github官网的图
        dup2(cgi_output[1], STDOUT);
        dup2(cgi_input[0], STDIN);
        close(cgi_output[0]);
        close(cgi_input[1]);

        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        // putenv(): 以name=value的形式设置 cgi 环境变量:
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            // 设置query_string的环境变量
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            // 设置length_env的环境变量
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        // 将子进程替换成另一个进程并执行cgi 脚本执,获取cgi的标准输出作为相应内容发送给客户端
        execl(path,  "",NULL);
        exit(0);
    } else {    /* parent */
        // 父进程关闭部分管道口，建立和子进程的通信
        close(cgi_output[1]);
        close(cgi_input[0]);

        if (strcasecmp(method, "POST") == 0)
            // 接收POST传递过来的数据，并通过 cgi_input 传入子进程的标准输入
            for (i = 0; i < content_length; i++) {
                recv(client_sock, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }

        // 通过 cgi_output，获取子进程的标准输出，并将其写入到客户端
        while (read(cgi_output[0], &c, 1) > 0)
            send(client_sock, &c, 1, 0);

        // 关闭剩余的管道口
        close(cgi_output[0]);
        close(cgi_input[1]);
        // 等待子进程终止
        waitpid(pid, &status, 0);
    }
}

// @brief: 实现读取HTTP请求报文的一行
// @param 1: 需要读取的套接字
// @param 2: 保存读取的缓冲区:
// @param 3: 缓冲区目前使用的大小
// @return: 实际读取的字符数量
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);   // 一个一个字符接收
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);   // MSG_PEEK: 读取缓冲区但不删除缓冲区的数据
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);  
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

// 服务器端主函数，包含一个while循环监听端口
int main()
{
	u_short listen_port = 4000;	        
	int serv_listen_sock = InitialServerListenSock(listen_port);	
	
	int client_sock = -1;    		// 用来保存进行客户端连接的服务端接口
	struct sockaddr_in clint_addr;	// 用来保存客户端地址信息
	socklen_t client_addr_size = sizeof(clint_addr);
	
	pthread_t new_thread;	// 保存线程Id

	// 开始等待连接
	while(1){
	    client_sock = accept(serv_listen_sock, (struct sockaddr *)&clint_addr, &client_addr_size);
		if (client_sock == -1)
			ErrorHandling("accpet() error");
	
		// 一旦有客户端连接成功，创建一个线程去处理信息传输
		if (pthread_create(&new_thread, NULL, (void *)AcceptRequest, (void*)(&client_sock)) != 0)
			ErrorHandling("phtread_create() error"); 
		
		// 改进：线程执行完成，回收线程所有的资源
		pthread_detach(new_thread);
	}
	
	// 关闭服务器端对端口的监听
	close(serv_listen_sock);
	return 0;
}
// @brief: 根据指定的端口号，初始化服务器端监听该端口的套接字
// @param: 需要监听的端口号
// @return: 监听指定端口的服务器套接字的文件描述符
int InitialServerListenSock(u_short listen_port)
{
	// 创建套接字
	int serv_listen_sock  = socket(PF_INET, SOCK_STREAM, 0);	
	if (serv_listen_sock == -1)
		ErrorHandling("socket() error");

	// 初始化地址信息
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(listen_port);	
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 设置套接字端口可快速重用，不需要time-wait
	int on = 1;
	if((setsockopt(serv_listen_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)
		ErrorHandling("setsockopt() error");	
	
	// 套接字绑定监听的地址和端口
	if (bind(serv_listen_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		ErrorHandling("bind() error");
	
	// 设置等待队列中的等待连接数量
	if (listen(serv_listen_sock, 5) < 0)
		ErrorHandling("listen() error");
	
	return serv_listen_sock;	
}


// ----------------------错误处理函数-----------------------------------
// @brief: 返回501响应消息
// @param 1: 发起请求的客户端套接字
// @return: void
void BadRequest(int client_sock)
{
    char buf[1024];
    strcat(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    strcat(buf, "Content-type: text/html\r\n\r\n<P>Your browser sent a bad request, such as a POST without a Content-Length.\r\n");
    send(client_sock, buf, sizeof(buf), 0);
}
// @brief: 返回500响应消息
// @param 1: 发起请求的客户端套接字
// @return: void
void CannotExecute(int client_sock)
{
    char buf[1024];
    strcat(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    strcat(buf, "Content-type: text/html\r\n\r\n<P>Error prohibited CGI execution.\r\n");
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client_sock, buf, strlen(buf), 0);
}

// @brief: 向客户端返回错误信息
// @param 1: 客户端套接字的文件描述符
// @return: void
void UnimplementedMethod(int client_sock) {
	char buf[1024];
	strcat(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
	strcat(buf, SERVER_STRING);
	strcat(buf, "Content-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>Method Not Implemented\r\n</TITLE></HEAD>\r\n<BODY><P>HTTP request method not supported.\r\n</BODY></HTML>\r\n");
	printf("%s\n",buf);
	send(client_sock, buf, strlen(buf), 0);	// 最后将构造的HTTP响应信息发送给客户端
}
// @brief: 返回404信息
// @param 1: 客户端套接字
void NotFound(int client_sock)
{
	// 修改成一次发送
	char buf[1024] = "HTTP/1.0 404 NOT FOUND\r\n";
	strcat(buf, SERVER_STRING);
	strcat(buf, "Content-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>Not Found\r\n</TITLE></HEAD>\r\n<BODY><P>resource is unavailable or nonexistent.\r\n</BODY></HTML>\r\n");
    send(client_sock, buf, strlen(buf), 0);
}
// @brief: 向stderr中输出错误信息
// @param 1: 保存有具体错误信息的字符串
// @return:  void
void ErrorHandling(const char *msg)
{
	perror(msg);
	exit(1);
}
