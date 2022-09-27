#ifndef LOCKER_H
#define LOCKER_H
#include <exception> //异常处理
#include <pthread.h> //线程
#include <semaphore.h> //信号量

//在这个文件中
//重新构造了信号量sem 互斥锁lock 条件变量cond 这几个线程通信工具。

class sem
{
public:
    sem()
    {
        if(sem_init(&m_sem,0, 0) != 0){
            throw std::exception();
        }
    }
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
private:
    pthread_mutex_t m_mutex;
};

#endif
