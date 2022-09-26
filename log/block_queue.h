/*****************
 * 用循环数组实现阻塞队列, m_back = (m_back + 1) % m_max_size() 简单的
 * 线程安全， 每个操作前都要加互斥锁，操作完后，再解锁
 * 
 * 天啊，下边全是由线程们共享的进程的资源啊，全都得加锁啊，跟雷区一样啊，小心翼翼
 * **************/

#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"
using namespace std;

#endif
