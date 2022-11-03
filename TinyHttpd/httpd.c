/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

// void accept_request(void *);     修改1: 为了使用pthread_create()函数
void* accept_request(void *);

void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
/*
    每个连接客户端的线程都执行该函数
Args: 
    服务器端和客户进行连接的套接字
Returns:
    void    
*/
// void accept_request(void *arg)   修改1: 为了使用pthread_create()函数
void* accept_request(void *arg)
{
    int client = (intptr_t)arg;    // 获得服务器端和客户进行连接的套接字
    char buf[1024];
    size_t numchars;
    // 保存从http请求消息中获得的信息
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;      /* becomes true if server decides this is a CGI
                       * program */
    char *query_string = NULL;

    // 获得HTTP请求头的请求行
    numchars = get_line(client, buf, sizeof(buf));
    i = 0; j = 0;
    // 读取请求的方法
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))    // 以空格结尾
    {
        method[i] = buf[i];
        i++;
    }
    j=i;
    method[i] = '\0';
    // http只能是GET或POSTf方法
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);
        // return;      修改1：
        return NULL;
    }
    // 如果是POST方法, 则开启CGI
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    // 跳过多余的空格
    while (ISspace(buf[j]) && (j < numchars))
        j++;
    // 获得HTTP请求头的文件地址
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    if (strcasecmp(method, "GET") == 0) // 如果是GET方法
    {
        query_string = url;
        // 检查query_string中是否存在 ? 
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        // Get中,?后面是参数
        if (*query_string == '?')
        {
            cgi = 1;        // 开启cgi
            *query_string = '\0';
            query_string++;
        }
    }
    //从TinyHTTPd项目的htdocs目录下提供文件
    sprintf(path, "htdocs%s", url);
    if (path[strlen(path) - 1] == '/')  // 如果是目录类型，
        strcat(path, "index.html"); // 则默认访问index.html
    if (stat(path, &st) == -1) {    // 如果找不到文件
        // 读取且销毁消息头
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);  // 向客户返回404信息
    }
    else    // 如果找到文件
    {
        if ((st.st_mode & S_IFMT) == S_IFDIR)   // 如果该文件是个目录，则默认使用它下面的index.html文件
            strcat(path, "/index.html");
        // 如果文件有可执行权限，开启 cgi
        if ((st.st_mode & S_IXUSR) ||
                (st.st_mode & S_IXGRP) ||
                (st.st_mode & S_IXOTH)    )
            cgi = 1;

        if (!cgi)   // 如果是静态页面
            serve_file(client, path);
        else       // 如果是动态页面请求
            execute_cgi(client, path, method, query_string);
    }

    close(client);
    return NULL;    // 修改1： 
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
/*
    将resource描述符指定文件中的数据发送给client
*/
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);     // 按行读取
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);  
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
/*
    将请求传递给cgi程序处理
*/
void execute_cgi(int client, const char *path,
        const char *method, const char *query_string)
{
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A'; buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else if (strcasecmp(method, "POST") == 0) /*POST*/
    {
        // 如果是POST请求，就需要得到 Content-Length
        // Content-Length 字符串长度为15
        // 从 17 位开始是具体的长度信息
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));
            numchars = get_line(client, buf, sizeof(buf));  // 其他信息读取并
        }
        if (content_length == -1) { 
            bad_request(client);    ////请求的页面数据为空，没有数据，就是我们打开网页经常出现空白页面
            return;
        }
    }
    else/*HEAD or other*/
    {
    }

    // 建立管道
    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }
    // 创建子进程
    if ( (pid = fork()) < 0 ) {
        cannot_execute(client);
        return;
    }
    // 状态行
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

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
        execl(path, NULL);
        exit(0);
    } else {    /* parent */
        // 父进程关闭部分管道口，建立和子进程的通信
        close(cgi_output[1]);
        close(cgi_input[0]);

        if (strcasecmp(method, "POST") == 0)
            // 接收POST传递过来的数据，并通过 cgi_input 传入子进程的标准输入
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }

        // 通过 cgi_output，获取子进程的标准输出，并将其写入到客户端
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        // 关闭剩余的管道口
        close(cgi_output[0]);
        close(cgi_input[1]);
        // 等待子进程终止
        waitpid(pid, &status, 0);
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
/*
    读取消息头，但是不要包括行末尾的转义字符(\r,\n,\r\n)
*/
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

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
/*
    返回HTTP响应头的状态行+消息头
*/
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    // 处理消息头
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");    // 以只读的方式打开文件
    if (resource == NULL)
        not_found(client);
    else
    {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
/*
    根据传输的端口号，创建服务器端的监听套接字
Args:
    指向保存端口号变量的指针
Returns:
    创建的sock套接字的文件描述符
*/
int startup(u_short *port)
{
    int httpd = 0;      
    int on = 1;
    struct sockaddr_in name;

    // 创建套接字
    httpd = socket(PF_INET, SOCK_STREAM, 0);    
    if (httpd == -1)
        error_die("socket");

    // 初始化套接字监听的地址信息
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    // 设置套接字httpd处于Time-Wait状态下的端口可以立即重新分配(on=1)
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)  
    {  
        error_die("setsockopt failed");
    }
    // httpd套接字绑定地址信息
    // 如果传进去的sockaddr结构中的 sin_port 指定为0，这时系统会选择一个临时的端口号
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    
    // 如果是动态分配的端口
    if (*port == 0)  /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);   //  获得系统分配的端口
    }
    // httpd套接字开始监听，设置队列中最终有5个
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
/*
    向客户端返回无法处理的消息
*/
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");    // 状态行
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);        // 消息头：服务器信息
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");    // 消息头： 内容类别
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");       // 消息头和消息体之间的空行
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
// 服务器端
int main(void)
{
    int server_sock = -1;
    u_short port = 4000;        // 服务器套接字监听的端口
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t  client_name_len = sizeof(client_name);
    pthread_t newthread;

    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        //服务器端监听套接字等待连接
        client_sock = accept(server_sock,
                (struct sockaddr *)&client_name,
                &client_name_len);
        if (client_sock == -1)
            error_die("accept");
        /* accept_request(&client_sock); */
        // 一旦有客户端连接，就创建线程处理
        if (pthread_create(&newthread , NULL, (void *)accept_request, (void *)(intptr_t)client_sock) != 0)
            perror("pthread_create");
    }
    // 关闭服务器监听套接字
    close(server_sock);

    return(0);
}
