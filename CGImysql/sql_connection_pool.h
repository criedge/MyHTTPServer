/*****
 * 数据库连接池+数据库用户校验
 * 单例模式
 * 用列表实现连接池
 * 连接池静态大小
 * 用互斥锁实现线程安全
 *
 * 校验
 * 支持POST的HTTP请求
 * 校验用户名和密码是否匹配
 * 保证多线程用户注册的安全性
 * ***/


#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <string>

#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool
{
public:
    MYSQL *GetConnection();                 //获取数据库连接
    bool ReleaseConnection(MYSQL *conn);    //释放连接
    int GetFreeConn();                      //获取连接
    void DestroyPool();                     //销毁连接池，释放所有连接  

    static connection_pool *GetInstance();

    void init(  string url, string User, string PassWord, string DataBaseName,
                int Port, int MaxConn, int close_log);

private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;
    int m_CurConn;
    int m_FreeConn;
    locker lock;
    list<MYSQL *> connList;
    sem reserve;

public:
    string m_url;           //主机地址
    string m_Port;          //数据库端口号
    string m_User;          //数据库用户名
    string m_PassWord;      //用户密码
    string m_DatabaseName;  //使用的数据库，base的b居然没有大写
    int m_close_log;        //日志开关
};

class connectionRAII
{
public:
    connectionRAII(MYSQL **con, connection_pool *connPool);
    ~connectionRAII();

private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};
#endif