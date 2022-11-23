#include<stdio.h>
#include<stdlib.h>  // exit()
#include<string.h>
#include<getopt.h>  // 提供getopt_long()
#include<unistd.h>  // pipe();fork();alarm()
#include<sys/types.h>   // pid_t类型
#include<sys/wait.h>
#include<sys/socket.h>


const char kVersion[] = "1.5";

// 所有函数声明
static int Bench(int client_name, int bench_time,int force);
void BuildRequest(const char *url);
void benchcore(const char *host, const int port, const char *req, int bench_time,int force);
static void Help_info();

// 静态变量
// long_options是getopt_long的参数，指明该命令可以支持的长选项
static const struct option long_options[] = {
    {"force", no_argument, NULL,'f'},      // 默认等待响应,设置该选择则返回'f'表示不等待服务器响应
    {"time", required_argument, NULL, 't'},
    {"help", no_argument, NULL, '?'},
    {"get", no_argument, NULL, 'g'},
    {"version", no_argument, NULL, 'V'},
    {"clients", required_argument, NULL, 'c'},
    {NULL, 0, NULL, 0}
};
static char http_request_msg [2048] ={0,};   // HTTP请求报文长度不超过2048
static char server_name[64] ={0,};    // Currently, the value of MAXHOSTNAMELEN is 64
static int port = 80;

volatile static int is_time_expirted = 0;
static int bytes_from_server = 0;  
static int success_client = 0;      

int main(int argc, char **argv) 
{
    // 如果没有输入参数，则输出命令的提示信息并且结束程序运行
    if (argc == 1) {
        Help_info();
        return 2;
    }
    
    // 默认选项的参数、
    int force = 0;          // 默认等待服务响应
    int bench_time = 30;    // 默认压测时间
    int client_num = 1;     // 默认并发数目

    // 下面两个变量帮助读取命令选项
    int long_index = 0;
    int opt = 0;
    while ((opt = getopt_long(argc, argv,"ft:c:?hV",long_options,&long_index)) != -1)
    {
        // 根据选项，更改设置
        switch (opt)
        {
            case 'f':
                force = 1;
                break;
            case 't':
                bench_time = atoi(optarg);
                break;
            case 'c':
                client_num = atoi(optarg);
                break;
            case '?':
            case 'h':
		Help_info();
                return 2;
                break;
            case 'V':
                printf("%s \n", kVersion);
                exit(0);
            default:
                break;
        }
    }

    if (client_num <= 0)
        client_num = 1;
    if (bench_time <= 0)
        bench_time = 30;

    // 处理完选项, 现在处理真正需要的参数
    if (optind == argc) {
        perror("webbench: Missing URL!\n");
        Help_info();
        return 2;
    }

    perror("Webbench - Simple Web Benchmark \n");

    BuildRequest(argv[optind]);  // 开始压测

    printf("Running info: %d clients, running %d sec\n", client_num, bench_time);

    return Bench(client_num, bench_time, force);
} 

// @brief: 根据clinets的数量，创建子进程进行压测
// @param 1: 同时发送连接的客户端数量
// @param 2: 压测时间
// @return: 返回执行是否成功
static int Bench(int client_num, int bench_time, int force) {

    // Socket()是socket.c中提供的创建客户端连接套接字的函数
    int tmp_sock = Socket(server_name , port);
    if(tmp_sock == -1) {
        perror( "\nConnect to server failed. Aborting benchmark.\n");
        return 1;
    };
    close(tmp_sock);    // 释放资源

    int my_pipe[2];
    if(pipe(my_pipe)) {
        perror( "pipe failed.");
        return 1;
    }
    pid_t pid = 0;
    for (int i = 0; i < client_num; ++i) {
        pid = fork();
        if (pid < 0) {
            fprintf(stderr, "problems forking worker no. %d\n", i);
            perror("fork failed.");
            return 3;
        } else if (pid == 0) {  // 子进程
            benchcore(server_name, port, http_request_msg, bench_time, force);
            close(my_pipe[0]);
            FILE *f = fdopen(my_pipe[1], "w");

            if (f == NULL)
            {
                perror("open pipe for writing failed.");
                return 3;
            }

            /* fprintf(stderr,"Child - %d %d\n",speed,failed); */
            // 每个子进程写入自己的结果
            fprintf(f, "%d %d\n", success_client, bytes_from_server);
            fclose(f);

            return 0;
        } 
    }

    // 父进程执行到这里需要等待所有的子进程完成压测工作才行
    int status = 0;
    while(wait(&status) > 0) {}

    close(my_pipe[1]);
    FILE *f = fdopen(my_pipe[0], "r");
    if (f == NULL)
    {
        perror("open pipe for reading failed.");
        return 3;
    }

    setvbuf(f, NULL, _IONBF, 0);    // 设置无缓冲

    while(1) {
        int  fork_success, fork_byte;
        int param_num = fscanf(f, "%d %d", &fork_success, &fork_byte);
        if (param_num != 2)
        {
            fprintf(stderr, "Some of our childrens died.\n");
            break;
        }
        // 统计数据,
        // 父进程总结所有子进程的数据
        success_client += fork_success;
        bytes_from_server += fork_byte;

        /* fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid); */
        if (--client_num == 0)
            break;
    }

    fclose(f);  

    printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
               (int)((client_num) / (bench_time / 60.0f)),
               (int)(bytes_from_server / (float)bench_time),
               success_client,
               client_num - success_client);
    return 0;

}

static void alarm_handler(int signal)
{
    // timerexpired=1会使得代码跳出循环
    is_time_expirted = 1;
}
// @brief: 每个进程在该函数内向服务端发起连接
// @param 1: 服务器的域名
// @param 2: 服务器监听的端口
// @param 3: 需要发送的http请求报文信息
void benchcore(const char *host, const int port, const char *req,int bench_time, int force) {

    // 该函数的运行时间就是bench_time, 利用alarm来监控
    struct sigaction sa;   
    sa.sa_handler = alarm_handler;
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL))
        exit(3);
    alarm(bench_time);

    int len = strlen(req);
    int fail = 0;   // 记录该客户端是否连接成功
    while (!is_time_expirted)
    {
        int s = Socket(host,port);
        if (s < 0) {    // 连接套接字创建失败
            fail = 1;
            continue;
        }

        if (len != write(s, req, len)) {    // http请求报文发送失败
            fail = 1;   
            close(s);
            continue;
        }

        int j = 0;
        if (force == 0) {     // 如果设置force, 则需要先读取http响应报文
            while (1)
            {
                if (is_time_expirted)
                    break;
                char buf[1500] = {0,};
                j = read(s, buf, 1500);
                if (j < 0) {
                    fail = 1;
                    break;
                } else if (j == 0) {
                    break;
                } else
                    bytes_from_server += j;
                
            }
            
        }

        if(close(s)) {
            fail = 1;
            continue;
        }

        if (!j)     // 如果j!=0, 则说明没有读取成功响应报文，也认为失败
            success_client++;      // 记录成功连接的的客户端数量
    }
    
    
}
// @brief: 根据选项参数，利用fork()进行实际的压测
// @param 1: 目的服务器的URL
void BuildRequest(const char *url) {
    
    // 判断URL的合法性
    if (0 != strncasecmp("http://", url, 7)) {
        fprintf(stderr, "\n%s: is not a valid URL.\n", url);
        exit(2);
    }
    if (strlen(url) > 1500) {      // 2.url过长
        fprintf(stderr, "URL is too long.\n");
        exit(2);
    }
    
    // 开始构造http请求报文请求行
    strcpy(http_request_msg, "GET ");

    int host_addr_start_index = strstr(url, "://") - url + 3;
    int i = host_addr_start_index;
    int j = 0;

    while(i <= strlen(url) && (url[i] != ':' || url[i] != '/')) {
        server_name[j++] = url[i ++];
    }

    i++;
    j = 0;
    char tmp[10] = {0,};
    while(i <= strlen(url) && url[i] != '/') {
        tmp[j++] = url[i++];
    }
    if(atoi(tmp) != 0)
        port = atoi(tmp);
    
    if (i != strlen(url))
        strcat(http_request_msg, url + i);
    else
        strcat(http_request_msg, "/index.html");
    
    strcat(http_request_msg, " HTTP/1.1");

    // 请求行填充结束，加入换行\r\n
    strcat(http_request_msg, "\r\n");

    // 开始构造请求头部(只构造必要的字段)
    strcat(http_request_msg, "Host: ");
    strcat(http_request_msg, server_name);
    strcat(http_request_msg, "\r\n");
    // 中间服务器不需要缓存
    strcat(http_request_msg, "Cache-Control: no-cache");
    // 取消http1.1默认的保持连接设置
    strcat(http_request_msg, "Connection: close\r\n");

    strcat(http_request_msg, "\r\n");

    printf("\nRequest:\n%s\n", http_request_msg);    // 显示请求报文信息

}

// @brief: 输出提示信息
static void Help_info() {
    perror("webbench [option]... URL\n"
            "  -f|--force               Don't wait for reply from server.\n"
            "  -r|--reload              Send reload request - Pragma: no-cache.\n"
            "  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
            "  -p|--proxy <server:port> Use proxy server for request.\n"
            "  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
            "  -9|--http09              Use HTTP/0.9 style requests.\n"
            "  -1|--http10              Use HTTP/1.0 protocol.\n"
            "  -2|--http11              Use HTTP/1.1 protocol.\n"
            "  --get                    Use GET request method.\n"
            "  --head                   Use HEAD request method.\n"
            "  --options                Use OPTIONS request method.\n"
            "  --trace                  Use TRACE request method.\n"
            "  -?|-h|--help             This information.\n"
            "  -V|--version             Display program version.\n");
}
