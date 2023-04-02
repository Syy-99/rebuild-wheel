/*
数据库连接池
- 单例模式：保证唯一
- list实现连接池管理
- 连接池的大小是固定的
- 通过互斥锁实现线程安全
*/

#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <iostream>
#include <cstring>
#include <list>
#include <mysql/mysql.h>        // 提供Mysql连接的数据结构
#include "../lock/locker.h"

using std::string;
using std::list;
using std::cout;


class connection_pool
{
public:
	MYSQL *GetConnection();				 //获取数据库连接
	bool ReleaseConnection(MYSQL *conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

    // 单例模式，获得实例的接口
    static connection_pool *GetInstance();

    void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn); 

    connection_pool();
	~connection_pool();

private:
    // 记录连接的数据库情况
    // 这一部分的属性对每个连接都是一样的（因为都是访问同一个主机上的同一个数据库）
    string  url_;    // 主机地址
    string port_;    // 数据库端口号
    string user_;    // 登录数据库的用户名
    string password_;   // 登录数据库的密码
    string database_name_;   // 使用的数据库名

private:
    locker lock_;   // 互斥锁
    sem reserve;      // 信号量实现多线程争夺连接

    list<MYSQL *>  connect_list_;    // 连接池, 保存了mysql连接的信息
    unsigned int max_conn_;  //最大连接数
	unsigned int used_conn_;  //当前已使用的连接数
	unsigned int free_conn_; //当前空闲的连接数

};


// RAII机制释放数据库连接
class connectionRAII{

public:
	connectionRAII(MYSQL **con, connection_pool *connPool);
	~connectionRAII();
	
private:
	MYSQL *conRAII;     // 这个类管理的数据库连接
	connection_pool *poolRAII;  // 连接对应的连接池（指明了该数据库连接应该放回的位置）
};


#endif