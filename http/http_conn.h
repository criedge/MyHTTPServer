#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h> //UNIX_STD C 和 C++ 程序设计语言中提供对 POSIX 操作系统 API 的访问功能的头文件的名称
#include <signal.h> //是C标准函数库中的信号处理部分
#include <sys/types.h> //各种各样的数据类型
#include <sys/epoll.h> //epoll
#include <fcntl.h> //FILE_CONTROL 提供unix标准中通用的文件控制函数
#include <sys/socket.h> //SOCKET 编程
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h> //线程库
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
//麻了呀 敲完还是没有找到string 看看下边四个本地头文件里有吗
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

#endif