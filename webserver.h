//第一个文件 webserver.cpp的头文件webserver.h

#ifndef WEBSERVER_H
#define WEBSERVER_H

//socket和epoll
#include <sys/socket.h>
#include <sys/epoll.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <cassert>
#include <unistd.h>
//经典IO
#include <stdlib.h>
#include <stdio.h>

//本地实现的线程池和http连接解析的头文件
//抄到这里顺手创建了文件，但是目录空的
#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

//**** 自行配置的头文件 ****//

//配置常量
const int MAX_FD = 65536; //文件描述符的最大值
const int MAX_EVENT_NUMBER = 10000; //最大事件数 和epoll相关
const int TIMESLOT = 5; //最小超时单位 单位：秒

class WebServer
{
public:
    WebServer();
    ~WebServer();
    //抄到这里 string类型报错。赶快回include找哪里引用了string,结果只找到string.h
    //我不道啊？
    //解决——把所有头文件引用的头文件都敲了一遍之后，找到了，在/log/log.h里声明
    //../CGImysql/sql_connection_pool.h也有string声明
    void init(int port, string user, string passWord, string databaseNamem,
              int log_write, int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);
        
    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd, struct sockaddr_in client_address);
    //抄到这里找不到util_timer了 回头找那两个本地的include
    //一层层头文件敲，终于找到声明的地方了 在/timer/lst_timer.h
    //util_timer是项目自行定义实现的一个类
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);
    bool dealclientdata();
    bool dealwithsignal(bool& timerout, bool& stop_server);
    void dealwiththread(int socckfd);
    void dealwithwrite(int sockfd);
public:
    int m_port; // 服务器端口
    char *m_root; // 根目录
    int m_log_write; // 日志类型
    int m_close_log; // 是否启动日志
    int m_actormodel; // Reactor, Proactor
    int m_pipefd[2]; //相互连接的套接字
    int m_epollfd; //epoll对象
    //到这里回头去声明http_conn类
    http_conn *users; //每个对象存储并管理一个http连接

    //到这里回头去声明connection_pool
    //可以预见，对应的cpp文件肯定要操作类的成员
    //到时候敲cpp文件再补具体实现吧
    connection_pool *m_connPool;
    string m_user; //数据库连接的用户名
    string m_passWord; //数据库连接的密码
    string m_databaseName; //数据库连接使用的库名
    int m_sql_num;

    //线程池相关
    //居然使用模板定义 非常的高级
    //回头敲threadpool类的头文件
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event相关：
    //epoll模块 负责IO，从listen()获取客户端连接fd，处理连接
    //
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd; //监听套接字
    int m_OPT_LINGER;// 是否优雅下线
    int m_TRIGMode; // ET/LT
    int m_LISTENTrigmode; // ET/LT
    int m_CONNTrigmode; // ET/LT

    //定时器
    //回头声明client_data
    //lst_timer.h里的一个结构体
    client_data *users_timer;

    //工具类
    //回头声明Utils类
    //lst_timer.h里的一个类
    Utils utils;
};

#endif