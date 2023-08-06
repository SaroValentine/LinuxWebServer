#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <pthread.h>
#include <list>
#include <cstdio>
#include <exception>
#include "lock.h"
// 线程池类,定义成模板类是为了代码复用，模板参数T是任务类
template <typename T>

class threadpool
{
private:
    // 线程池中的线程数
    int m_thread_number;

    // 描述线程池的数组，其大小为m_thread_number
    pthread_t *m_threads;

    // 请求队列中最多允许的、等待处理的请求的数量
    int m_max_requests;

    // 请求队列
    std::list<T *> m_workqueue;

    // 保护请求队列的互斥锁
    locker m_queuelocker;

    // 是否有任务需要处理,定义信号量
    sem m_queuestat;

    // 是否结束线程
    bool m_stop;

private:
    static void *worker(void *arg);
    void run();

public:
    // 构造函数
    threadpool(int thread_number = 8, int max_requests = 10000);

    // 析构函数
    ~threadpool();

    // 往请求队列中添加任务
    bool append(T *request);
};

template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL)
{
    if ((thread_number <= 0) || (max_requests <= 0))
    {
        throw std::exception();
    }

    // 创建线程数组
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }

    // 创建thread_number个线程，并将它们都设置为脱离线程
    for (int i = 0; i < thread_number; ++i)
    {
        printf("create the %dth thread\n", i);
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }

        // 将线程设置为脱离线程
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool threadpool<T>::append(T *request)
{
    // 操作工作队列时一定要加锁，因为它被所有线程共享
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }

    // 将任务添加到工作队列中
    m_workqueue.push_back(request);
    m_queuelocker.unlock();

    // 增加信号量的值，有任务需要处理
    m_queuestat.post();
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{
    // 将参数强制转换为线程池对象
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        // 等待信号量，有任务需要处理
        m_queuestat.wait();
        // 操作工作队列时一定要加锁，因为它被所有线程共享
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        // 取出任务队列中的第一个任务
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
        {
            continue;
        }

        // 处理任务
        request->process();
    }
}

#endif