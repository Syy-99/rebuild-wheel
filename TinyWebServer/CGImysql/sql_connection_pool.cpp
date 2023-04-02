#include "sql_connection_pool.h"


connection_pool::connection_pool() 
{
	max_conn_ = 0;
	used_conn_ = 0;
   	free_conn_ = 0;
   // 其他数据成员要么无法默认初始化，要么会自动调用构造函数
}

// 单例模式获得对象
connection_pool *connection_pool::GetInstance()
{
	static connection_pool connPool;
	return &connPool;
}

//构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn)
{
	this->url_ = url;
	this->port_ = Port;
	this->user_ = User;
	this->password_ = PassWord;
	this->database_name_ = DBName;

	lock_.lock();
    // 初始化连接池中的连接的数量
	for (int i = 0; i < MaxConn; i++)
	{
		MYSQL *con = NULL;
		con = mysql_init(con);	// 初始化一个MySQL连接

		if (con == NULL)
		{
			cout << "Error:" << mysql_error(con);
			exit(1);
		}
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

		if (con == NULL)
		{
			cout << "Error: " << mysql_error(con);
			exit(1);
		}
        // 到这里，说明已经有一个MySQL连接被建立

		connect_list_.push_back(con);   // 将该MySQL连接保存到链表中
		++free_conn_;
	}

	reserve = sem(free_conn_);  // 根据初始化的连接数量，初始化信号量的值

	this->max_conn_ = free_conn_;
	
	lock_.unlock();
}

// 当有请求时，从数据库中返回一个数据库连接
MYSQL *connection_pool::GetConnection() 
{
    MYSQL *con = nullptr;

    if (0 == connect_list_.size()) {  //? 有问题吧? 感觉和下面逻辑冲突了
        return nullptr;     
    }

    reserve.wait();     // 更新信号量，如果为0，则阻塞

    lock_.unlock();
    con = connect_list_.front();    // 从链表头部获得一个连接
    connect_list_.pop_front();      // 在链表中删除该连接的指针（注意是指针)

    --free_conn_;
	++used_conn_;

    lock_.unlock();
	return con;
    // 注意这里没有调用post()更新信号量
}

// 释放当前使用的连接，回到连接池中
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	if (NULL == con)
		return false;

	lock_.lock();

	connect_list_.push_back(con);
	++free_conn_;
	--used_conn_;

	lock_.unlock();

	reserve.post();
	return true;
}


//销毁数据库连接池
void connection_pool::DestroyPool()
{

	lock_.lock();
	if (connect_list_.size() > 0)
	{
		list<MYSQL *>::iterator it;
		for (it = connect_list_.begin(); it != connect_list_.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);
		}
		used_conn_ = 0;
		free_conn_ = 0;
		connect_list_.clear();

		lock_.unlock();
	}

	lock_.unlock();
}


//当前空闲的连接数
int connection_pool::GetFreeConn()
{
	return this->free_conn_;
}

connection_pool::~connection_pool()
{
	DestroyPool();
}


connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
	*SQL = connPool->GetConnection();
	// 从数据库连接池中取出一个连接
	conRAII = *SQL;
	poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);	// 将连接放回数据库连接池中
}


