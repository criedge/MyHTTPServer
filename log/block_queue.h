/*****************
 * 用循环数组实现阻塞队列, m_back = (m_back + 1) % m_max_size() 
 * 线程安全， 每个操作前都要加互斥锁，操作完后，再解锁
 * 阻塞队列是为了实现异步日志写入
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

template<class T>
class block_queue
{
public:
    block_queue(int max_size = 1000)
    {
        if(max_size <= 0)
        {
            exit(-1);
        }

        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        //虽然说队列是先进先出
        //但是实现上,数据进入(push)时队尾back++, 数据输出(pop)时队头front++
        //因此back总是逻辑上在front前边
        //实现上由于是循环队列, 逻辑上领先的back可能下标比front小, 类似于领先了一圈吧
        m_front = -1;
        m_back = -1;
    }

    void clear()
    {
        m_mutex.lock();
        m_size = 0;
        //头尾指针都指向当前元素的上一个位置
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    ~block_queue()
    {
        m_mutex.lock();
        if(m_array != NULL){
            delete [] m_array;
        }
        m_mutex.unlock();
    }

    //判断队列是否为满
    //m_size是互斥资源，因此访问m_size时需要加锁
    bool full()
    {
        m_mutex.lock();
        if(m_size >= m_max_size)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    //判断队列是否为空
    //m_size是互斥资源
    bool empty()
    {
        m_mutex.lock();
        if(0 == m_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    //返回队首资源
    bool front(T &value){
        m_mutex.lock();
        if(0 == m_size)
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    //返回队尾资源
    bool back(T &value)
    {
        m_mutex.lock();
        if(0 == m_size)
        {
            m.mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    //返回元素个数
    int size()
    {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_size;
        m_mutex.unlock();

        return tmp;
    }
    
    int max_size()
    {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_max_size;
        m_mutex.unlock();

        return tmp;
    }

    //往队列添加元素，需要将所有使用队列的线程先唤醒
    //当有元素push进队列，相当于生产者P+1
    //若当前没有线程等待条件变量，则唤醒无意义
    bool push(const T &item)
    {
        m_mutex.lock();
        if(m_size >= m_max_size)
        {
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }
        
        m_back = (m_back + 1) & m_max_size;
        m_array[m_back] = item;

        m_size++;

        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    bool pop(T &item)
    {
        m_mutex.lock();
        while(m.size <= 0)
        {
            if(!m.cond.wait(m_mutex.get()))
            {
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

    bool pop(T &item, int ms_timeout)
    {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        m_mutex.lock();
        if(m_size <= 0)
        {
            //计算秒单位的部分
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            //计算毫秒单位的部分
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if(!m_cond.timewait(m_mutex.get(), t))
            {
                m_mutex.unlock();
                return false;
            }
        }

        if(m_size <= 0)
        {
            m_mutex.unlock();
            return false;
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

private:
    locker m_mutex;
    cond m_cond;
    
    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
};
#endif
