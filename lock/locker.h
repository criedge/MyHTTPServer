#ifndef LOCKER_H
#define LOCKER_H
#include <exception> //异常处理
#include <pthread.h> //线程
#include <semaphore.h> //信号量

//在这个文件中
//重新构造了信号量sem 互斥锁lock 条件变量cond 这几个线程通信工具。

/**********************
 * 简单总结：
 * 整个过程捏，就是用自建类的成员函数包装了对应库的函数，定义必要的成员变量
 * 所以捏，很多成员函数都只有一行。
 * 为什么捏，
 * 是因为要用C++实现RAII(资源获取就是初始化),
 * 用构造函数和析构函数管理成员变量的生存期 
 *********************/
//信号量
class sem
{
public:
    //无参构造 初始化互斥信号量sem
    //sem_init()成功返回0 不成功返回-1并设置errno
    //throw 输出异常，并终止进程?线程?
    sem()
    {
        if(sem_init(&m_sem,0, 0) != 0){
            throw std::exception();
        }
    }
    //有参构造 初始化信号量sem 有资源num个
    sem(int num)
    {
        if( sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }
private:
    sem_t m_sem;
};

class locker
{
public:
    locker()
    {
        if(pthread_mutex_init(&m_mutex, NULL) != 0){
            throw std::exception();
        }
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;
};

//C++11 条件变量不需要手动实现m_mutex管理共享资源
//如果是之前版本的C++，需要cond类注释掉的代码，手动管理锁
class cond
{
public:
    cond()
    {
        if(pthread_cond_init(&m_cond, NULL) != 0)
        {
            //pthread_mutex_destroy(&m_mutex);
            throw::exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t *m_mutex)
    {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_wait(&m_cond, m_mutex);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }
private:
    //static pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};
#endif
