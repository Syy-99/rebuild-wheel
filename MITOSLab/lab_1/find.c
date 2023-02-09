#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"       // 提供O_RDONLY宏定义
#include "kernel/stat.h"        // 提供stat结构体
#include "kernel/fs.h"          // 提供dirent结构体

void find(char *dirctory_path, char *file_name) {
    // 根据路径打开文件
    int fd;
    if((fd = open(dirctory_path, O_RDONLY)) < 0 ) {     // ！！！注意这里的括号位置
        printf("cannot open %s", dirctory_path);
        exit(0);
    }

    // 获得文件信息
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("connot stat %s",dirctory_path);
        close(fd);
        exit(0);
    }

    switch (st.type)
    {
    case T_FILE:
        printf("%s is a file not directory", dirctory_path);
        break;
    case T_DIR:
        char buf[512], *p;
        strcpy(buf, dirctory_path);
        p = buf + strlen(dirctory_path);  // 以该目录为基础，开始向下查找
        *p++ = '/';    

        struct dirent de;
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            // if(de.inum == 0)        // 为什们innode number=0就直接跳过呢?
            //     continue;
            
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if(stat(buf, &st) < 0){
                printf("cannot stat %s\n", buf);
                continue;
            }
            // 文件类型和目录类型不同处理
            if (st.type == T_FILE && strcmp(de.name, file_name) == 0)
                printf("%s\n", buf);
            else if(st.type == T_DIR &&strcmp(de.name, ".") && strcmp(de.name, ".."))
                find(buf, file_name);
        }
        
    default:
        break;
    }
}
int main(int argc, char **argv) {

    if (argc < 3)
        printf("too few argument\n");
    else if (argc > 3)
        printf("too many arguemnt\n");
    else {
        find(argv[1], argv[2]);
    }

    exit(0);

}