#include "./log/log.h"






#define SYNLOG  //同步写日志
//#define ASYNLOG //异步写日志


int main(){

// 日志系统和服务器程序一起启动
#ifdef ASYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 8); //异步日志模型
#endif

#ifdef SYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 0); //同步日志模型
#endif

    return 0;
}