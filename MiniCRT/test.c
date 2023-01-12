#include "minicrt.h"

int main(int argc, char* argv[]) //莫名奇妙的形参只能传递使用一次
{
    int i;
    FILE* fp;
    char** v =(char**) malloc( argc * sizeof(char*) );
    char** tempArgv = argv;
    int tempArgc = argc;

    for(i=0; i<argc; ++i)
    {
        v[i] = (char*) malloc( strlen(tempArgv[i]) + 1);
        strcpy(v[i], tempArgv[i]);
    }

    fp = fopen("test.txt", "w");
    for(i = 0; i<tempArgc; ++i)
    {
        int len = strlen(v[i]);
        fwrite(&len, 1, sizeof(int), fp);
        fwrite(v[i], 1, len, fp);
    }
    fclose(fp);

    fp = fopen("test.txt", "r");
    for(i=0; i<tempArgc; ++i)
    {
        int len;
        char* buf;
        fread(&len, 1, sizeof(int), fp);
        buf =(char*) malloc(len + 1);
        fread(buf, 1, len, fp);
        buf[len] = '\0';
        printf("%d %s\n", len, buf);
        free(buf);
        free(v[i]);
    }
    fclose(fp);
}
