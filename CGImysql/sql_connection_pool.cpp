#include <mysql/mysql.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include<iostream>
#include <string>
#include <list>

#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
    //初始化当前线程池内的连接总数和空闲连接数
    m_CurConn = 0;
    m_FreeConn = 0;
}

//就是说 这里的细节 请见懒汉模式,懒汉模式多线程下初始化的隐患及C++11的静态变量多线程安全
connection_pool *connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

//构造初始化
void connection_pool::init( string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
    m_url = url;
    m_Port = Port;
    m_DatabaseName = DBName;
    m_PassWord = PassWord;
    m_close_log = close_log;

    //申请MaxConn个 与数据库的连接 把申请到的连接塞到一个队列里存好了
    //一步一回头，任何地方出错，都终止程序
    //这些连接 是服务器进程和数据库进程互相传输数据用的
    //由于服务器只使用了数据存储引擎的一个逻辑数据库的一个表
    //因此指定了访问的数据库名(这里指逻辑数据库DBName)，登录数据库(这里是MYSQL的数据存储引擎)的用户名和密码以及端口
    for(int i = 0; i < MaxConn; i++)
    {
        MYSQL *con = NULL;
        //在这里调用了系统库的mysql_init函数
        con = mysql_init(con);

        if(con == NULL)
        {
            //这里我把log.h里的宏扩展里的类名 Log 写成了LOG，报错了
            //报错信息：后面有“::”的名称一定是类名或命名空间名C/C++(276)
            //说明命名空间或者类名有错误
            LOG_ERROR("MYSQL Error");
            exit(1);
        }
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);
        
        if(con == NULL)
        {
            LOG_ERROR("MYSQL error");
            exit(1);
        }
        connList.push_back(con);
        ++m_FreeConn;
    }

    reserve = sem(m_FreeConn);

    m_MaxConn = m_FreeConn;
}

MYSQL *connection_pool::GetConnection()
{
    MYSQL *con = NULL;

    //线程池中没有空闲变量
    //这里查看connList.size()的大小
    //而不是m_FreeConn的值 为什么呢？
    if( 0 == connList.size())
    {
        return NULL;
    }
    reserve.wait();

    lock.lock();
    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();
    return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
    if(NULL == con){ return false;}

    lock.lock();

    connList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();
    //这里 P操作在解锁之后
    reserve.post();
    return true;
}

//销毁数据库连接池
void connection_pool::DestroyPool()
{
    lock.lock();
    if(connList.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for(it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }

        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }

    lock.unlock();
}

//查看当前空闲连接数
int connection_pool::GetFreeConn()
{
    return this->m_FreeConn;
}

connection_pool::~connection_pool()
{
    DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool)
{
    *SQL = connPool->GetConnection();

    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}