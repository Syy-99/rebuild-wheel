/*
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   webbench --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, fork failed
 *
 */

#include "socket.c"
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>

// 一些全局变量，用来保存值
volatile int timerexpired = 0; // 判断压测时长是否已经到达设定的时间
//测试结果
int speed = 0;  //成功得到服务器响应的子进程数量
int failed = 0; //没有成功得到服务器响应的子进程数量
int bytes = 0;  //所有子进程读取到服务器回复的总字节数

// 支持的HTTP版本
// 0 - http/0.9, 1 - http/1.0, 2 - http/1.1 */
int http10 = 1;

// 支持的HTTP请求方法
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3

#define PROGRAM_VERSION "1.5"   // 程序的版本信息

// 默认设置
int method = METHOD_GET; //请求方式：Get
int clients = 1;         // 并发数目为1，即只模拟1个客户端
int force = 0;           // 默认等待服务器响应
int force_reload = 0;    // 失败时重新请求
int proxyport = 80;      // 默认访问代理服务器端口80
char *proxyhost = NULL;  // 默认无代理服务器
int benchtime = 30;      // 默认压测时间30s

/* internal */
int mypipe[2];              //管道用于父子进程通信
char host[MAXHOSTNAMELEN];  //存储服务器地址
#define REQUEST_SIZE 2048   //最大请求次数
char request[REQUEST_SIZE]; //存放http请求报文信息数组

/*
设置命令的长选项
*/
static const struct option long_options[] =
    {

        {"force", no_argument, &force, 1},
        {"reload", no_argument, &force_reload, 1},
        {"time", required_argument, NULL, 't'},
        {"help", no_argument, NULL, '?'},
        {"http09", no_argument, NULL, '9'},
        {"http10", no_argument, NULL, '1'},
        {"http11", no_argument, NULL, '2'},
        {"get", no_argument, &method, METHOD_GET},
        {"head", no_argument, &method, METHOD_HEAD},
        {"options", no_argument, &method, METHOD_OPTIONS},
        {"trace", no_argument, &method, METHOD_TRACE},
        {"version", no_argument, NULL, 'V'},
        {"proxy", required_argument, NULL, 'p'},
        {"clients", required_argument, NULL, 'c'},
        {NULL, 0, NULL, 0}};

/* prototypes */
static void benchcore(const char *host, const int port, const char *request);
static int bench(void);
static void build_request(const char *url);

static void alarm_handler(int signal)
{
    timerexpired = 1;
}
/*
输出提示信息，显示命令支持的选项
*/
static void usage(void)
{
    fprintf(stderr,
            "webbench [option]... URL\n"
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

int main(int argc, char *argv[])
{
    // 读取命令行参数
    int opt = 0;
    int options_index = 0; // getopt_long()的第5个参数，一般为NULL

    char *tmp = NULL;

    if (argc == 1) // 如果没有输入额外的参数
    {
        usage(); // 则输出命令的帮助信息
        return 2;
    }

    // 开始处理命令行参数
    // getopt_long是命令行解析的库函数
    while ((opt = getopt_long(argc, argv, "912Vfrt:p:c:?h", long_options, &options_index)) != EOF)
    {
        // 根据返回值来判断选项的意义
        // 这部分的阅读建议一个一个选项来理解
        switch (opt)
        {
        case 0:
            break; // opt=0的情况，实际在long_options已经更改了默认设置
        case 'f':
            force = 1;
            break;
        case 'r':
            force_reload = 1;
            break;
        // 下面三个短参用来声明使用的HTTP协议
        case '9':
            http10 = 0;
            break;
        case '1':
            http10 = 1;
            break;
        case '2':
            http10 = 2;
            break;
        case 'V':
            printf(PROGRAM_VERSION "\n");
            exit(0);
        // 设置压测的时间，单位: s
        case 't':
            benchtime = atoi(optarg);
            break;
        // 设置代理信息，-p server:port
        case 'p':
            tmp = strrchr(optarg, ':');
            proxyhost = optarg;
            if (tmp == NULL) // 每找到 :
            {
                break;
            }
            if (tmp == optarg) // 选项参数形式如下 :port, 说明没有指定代理服务器地址
            {
                fprintf(stderr, "Error in option --proxy %s: Missing hostname.\n", optarg);
                return 2;
            }
            if (tmp == optarg + strlen(optarg) - 1) // 选项参数形式如下 server:, 说明没有指定代理服务器端口
            {
                fprintf(stderr, "Error in option --proxy %s Port number is missing.\n", optarg);
                return 2;
            }
            // 到这里，说明参数形式正确
            *tmp = '\0';               // 现在读proxyhost只会获得代理服务器地址，字符串形式
            proxyport = atoi(tmp + 1); // 获得代理服务器端口，整数形式
            break;
        // 下面三个短参都是输出帮助信息
        case ':':
        case 'h':
        case '?':
            usage();
            return 2;
            break;
        // 设置创建多少个客户端进行连接
        case 'c':
            clients = atoi(optarg);
            break;
        }
    }

    // 命令参数解析完毕之后，刚好是读到URL，此时argv[optind]指向URL
    // URL参数为空
    if (optind == argc) 
    {
        fprintf(stderr, "webbench: Missing URL!\n");
        usage();
        return 2;
    }

    // 如果使用命令选项修改clinets/benchtime，不可以设置为0
    if (clients == 0)
        clients = 1;    
    if (benchtime == 0)
        benchtime = 30; 

    // 程序说明
    fprintf(stderr, "Webbench - Simple Web Benchmark " PROGRAM_VERSION "\n"
                    "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n");
    
    // 构造请求报文
    build_request(argv[optind]);

    // print request info ,do it in function build_request
    /*printf("Benchmarking: ");

    switch(method)
    {
    case METHOD_GET:
    default:
    printf("GET");break;
    case METHOD_OPTIONS:
    printf("OPTIONS");break;
    case METHOD_HEAD:
    printf("HEAD");break;
    case METHOD_TRACE:
    printf("TRACE");break;
    }

    printf(" %s",argv[optind]);

    switch(http10)
    {
    case 0: printf(" (using HTTP/0.9)");break;
    case 2: printf(" (using HTTP/1.1)");break;
    }

    printf("\n");
    */

    printf("Runing info: ");

    if (clients == 1)
        printf("1 client");
    else
        printf("%d clients", clients);

    printf(", running %d sec", benchtime);

    if (force)
        printf(", early socket close");
    if (proxyhost != NULL)
        printf(", via proxy server %s:%d", proxyhost, proxyport);
    if (force_reload)
        printf(", forcing reload");

    printf(".\n");

    return bench();
}

/*
    构造http报文请求到request数组
Args:
    url: 服务器的地址信息
*/
void build_request(const char *url)
{
    char tmp[10];
    int i;

    // bzero(host,MAXHOSTNAMELEN);
    // bzero(request,REQUEST_SIZE);
    // 初始化
    memset(host, 0, MAXHOSTNAMELEN);
    memset(request, 0, REQUEST_SIZE);

     //判断应该使用的http协议(这部分更多是HTTP的知识)

    //1.缓存和代理都是都是http/1.0以后才有到的
    if (force_reload && proxyhost != NULL && http10 < 1)
        http10 = 1;
    //2.head请求是http/1.0后才有的
    if (method == METHOD_HEAD && http10 < 1)
        http10 = 1;
    //3.options请求和reace请求都是http/1.1才有     
    if (method == METHOD_OPTIONS && http10 < 2)
        http10 = 2;
    if (method == METHOD_TRACE && http10 < 2)
        http10 = 2;

    // 根据method填充请求报文的请求行
    switch (method)
    {
    default:
    case METHOD_GET:
        strcpy(request, "GET");
        break;
    case METHOD_HEAD:
        strcpy(request, "HEAD");
        break;
    case METHOD_OPTIONS:
        strcpy(request, "OPTIONS");
        break;
    case METHOD_TRACE:
        strcpy(request, "TRACE");
        break;
    }

    // 根据请求行的格式要求追加空格
    strcat(request, " ");

    // 判断URL的合法性
    // 1. url中没有 "://" 字符
    if (NULL == strstr(url, "://"))
    {
        fprintf(stderr, "\n%s: is not a valid URL.\n", url);
        exit(2);
    }
     //2.url过长
    if (strlen(url) > 1500)
    {
        fprintf(stderr, "URL is too long.\n");
        exit(2);
    }
    //3.若无代理服务器，则只支持http协议
    if (0 != strncasecmp("http://", url, 7))
    {
        fprintf(stderr, "\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
        exit(2);
    }

    /* protocol/host delimiter */
    i = strstr(url, "://") - url + 3;

    if (strchr(url + i, '/') == NULL)
    {
        fprintf(stderr, "\nInvalid URL syntax - hostname don't ends with '/'.\n");
        exit(2);
    }

    if (proxyhost == NULL)
    {
        /* get port from hostname */
        if (index(url + i, ':') != NULL && index(url + i, ':') < index(url + i, '/'))
        {
            strncpy(host, url + i, strchr(url + i, ':') - url - i);
            // bzero(tmp,10);
            memset(tmp, 0, 10);
            strncpy(tmp, index(url + i, ':') + 1, strchr(url + i, '/') - index(url + i, ':') - 1);
            /* printf("tmp=%s\n",tmp); */
            proxyport = atoi(tmp);
            if (proxyport == 0)
                proxyport = 80;
        }
        else
        {
            strncpy(host, url + i, strcspn(url + i, "/"));
        }
        // printf("Host=%s\n",host);
        strcat(request + strlen(request), url + i + strcspn(url + i, "/"));
    }
    else
    {
        // printf("ProxyHost=%s\nProxyPort=%d\n",proxyhost,proxyport);
        strcat(request, url);
    }

    if (http10 == 1)
        strcat(request, " HTTP/1.0");
    else if (http10 == 2)
        strcat(request, " HTTP/1.1");

    strcat(request, "\r\n");

    if (http10 > 0)
        strcat(request, "User-Agent: WebBench " PROGRAM_VERSION "\r\n");
    if (proxyhost == NULL && http10 > 0)
    {
        strcat(request, "Host: ");
        strcat(request, host);
        strcat(request, "\r\n");
    }

    if (force_reload && proxyhost != NULL)
    {
        strcat(request, "Pragma: no-cache\r\n");
    }

    if (http10 > 1)
        strcat(request, "Connection: close\r\n");

    /* add empty line at end */
    if (http10 > 0)
        strcat(request, "\r\n");

    printf("\nRequest:\n%s\n", request);
}

/* vraci system rc error kod */
static int bench(void)
{
    int i, j, k;
    pid_t pid = 0;
    FILE *f;

    /* check avaibility of target server */
    i = Socket(proxyhost == NULL ? host : proxyhost, proxyport);
    if (i < 0)
    {
        fprintf(stderr, "\nConnect to server failed. Aborting benchmark.\n");
        return 1;
    }
    close(i);

    /* create pipe */
    if (pipe(mypipe))
    {
        perror("pipe failed.");
        return 3;
    }

    /* not needed, since we have alarm() in childrens */
    /* wait 4 next system clock tick */
    /*
    cas=time(NULL);
    while(time(NULL)==cas)
    sched_yield();
    */

    /* fork childs */
    for (i = 0; i < clients; i++)
    {
        pid = fork();
        if (pid <= (pid_t)0)
        {
            /* child process or error*/
            sleep(1); /* make childs faster */
            break;
        }
    }

    if (pid < (pid_t)0)
    {
        fprintf(stderr, "problems forking worker no. %d\n", i);
        perror("fork failed.");
        return 3;
    }

    if (pid == (pid_t)0)
    {
        /* I am a child */
        if (proxyhost == NULL)
            benchcore(host, proxyport, request);
        else
            benchcore(proxyhost, proxyport, request);

        /* write results to pipe */
        f = fdopen(mypipe[1], "w");
        if (f == NULL)
        {
            perror("open pipe for writing failed.");
            return 3;
        }
        /* fprintf(stderr,"Child - %d %d\n",speed,failed); */
        fprintf(f, "%d %d %d\n", speed, failed, bytes);
        fclose(f);

        return 0;
    }
    else
    {
        f = fdopen(mypipe[0], "r");
        if (f == NULL)
        {
            perror("open pipe for reading failed.");
            return 3;
        }

        setvbuf(f, NULL, _IONBF, 0);

        speed = 0;
        failed = 0;
        bytes = 0;

        while (1)
        {
            pid = fscanf(f, "%d %d %d", &i, &j, &k);
            if (pid < 2)
            {
                fprintf(stderr, "Some of our childrens died.\n");
                break;
            }

            speed += i;
            failed += j;
            bytes += k;

            /* fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid); */
            if (--clients == 0)
                break;
        }

        fclose(f);

        printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
               (int)((speed + failed) / (benchtime / 60.0f)),
               (int)(bytes / (float)benchtime),
               speed,
               failed);
    }

    return i;
}

void benchcore(const char *host, const int port, const char *req)
{
    int rlen;
    char buf[1500];
    int s, i;
    struct sigaction sa;

    /* setup alarm signal handler */
    sa.sa_handler = alarm_handler;
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL))
        exit(3);

    alarm(benchtime); // after benchtime,then exit

    rlen = strlen(req);
nexttry:
    while (1)
    {
        if (timerexpired)
        {
            if (failed > 0)
            {
                /* fprintf(stderr,"Correcting failed by signal\n"); */
                failed--;
            }
            return;
        }

        s = Socket(host, port);
        if (s < 0)
        {
            failed++;
            continue;
        }
        if (rlen != write(s, req, rlen))
        {
            failed++;
            close(s);
            continue;
        }
        if (http10 == 0)
            if (shutdown(s, 1))
            {
                failed++;
                close(s);
                continue;
            }
        if (force == 0)
        {
            /* read all available data from socket */
            while (1)
            {
                if (timerexpired)
                    break;
                i = read(s, buf, 1500);
                /* fprintf(stderr,"%d\n",i); */
                if (i < 0)
                {
                    failed++;
                    close(s);
                    goto nexttry;
                }
                else if (i == 0)
                    break;
                else
                    bytes += i;
            }
        }
        if (close(s))
        {
            failed++;
            continue;
        }
        speed++;
    }
}
