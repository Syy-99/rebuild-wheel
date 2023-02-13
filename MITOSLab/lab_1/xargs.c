#include "user/user.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("the number of arguement is wrong\n");
        exit(0);
    }

    char *arguemet[33];     // 最多只可能有32个参数
    // 统计自带的参数
    for(int i = 1; i < argc; i++)
        arguemet[i - 1] = argv[i];

   
    char c;
    char buf[1024] = {0,};         // 假设参数长度不超1024
    int buf_index = 0;
    while(read(0, &c, sizeof(c))) {    // 从标准输入中读取
        if (c == '\n') {
            buf[buf_index++] = '\0';  // 手动添加字符串的终止标志
            if (fork() == 0) {
                    arguemet[argc - 1] = buf;
                    exec(arguemet[0], arguemet);
                    exit(0);
            } else {
                    buf_index = 0;      // 重新开始获得下一行的参数
                    wait(0);
            }    
        } else
             buf[buf_index++] = c;  
           
    }
    exit(0);
}