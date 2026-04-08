#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <cstdio>
#include <list>
#include <exception>

#include "locker.h"

using namespace std;

// 线程池类
template<typename T>
class threadpool
{
private:
    // 线程数量
    int m_thread_number;
    // 线程池数组
    pthread_t * m_threadpool;
    // 请求队列中最多允许的等待处理请求数量
    int m_maxrequestnum;
    // 请求队列
    list<T* > m_workqueue;
    // 互斥锁
    locker m_queuelocker;
    // 信号量 用来判断是否有任务需要处理
    sem m_queuestat;
    // 是否结束线程
    bool m_stop;
 
private:
    static void* worker(void*);
    void run();
public:
    threadpool(int thread_number = 8, int max_request = 10000);
    ~threadpool();

    // 添加任务
    bool append(T* request);
};


/**
 * @brief 构造线程池并创建工作线程
 * @param thread_number 线程池中线程数量（默认 8）
 * @param max_request 请求队列最大容量（默认 10000）
 * @throws exception 构造参数不合法或线程创建/分离失败时抛出
 */
template <typename T>
threadpool<T> :: threadpool(int thread_number, int max_request):
    m_thread_number(thread_number), m_maxrequestnum(max_request),
    m_threadpool(NULL), m_stop(false){
        if ((thread_number <= 0) || (max_request <= 0))
            throw exception();
        
        m_threadpool = new pthread_t[m_thread_number];
        if (!m_threadpool) throw exception();


        // 创建thread_num 个线程，并设置为detach
        for (int i = 0; i < thread_number; i++){
            printf("create %d thread\n", i);
            // worker 必须为static, this 传参
            if (pthread_create(m_threadpool + i, NULL, worker, this) != 0){
                delete [] m_threadpool;
                throw exception();
            }

            if (pthread_detach(m_threadpool[i])){
                delete [] m_threadpool;
                throw exception();                
            }
        }
}


/**
 * @brief 析构函数，清理线程数组并标记停止
 * @note 这里没有等待线程结束（线程创建时为 detached），需要确保其他资源可安全释放
 */
template <typename T>
threadpool<T> :: ~threadpool(){
    delete [] m_threadpool;
    m_stop = true;
}


/**
 * @brief 向线程池的任务队列添加一个任务指针
 * @param request 待处理任务（T*）
 * @return 添加成功返回 true；队列已满返回 false
 */
template <typename T>
bool threadpool<T> :: append(T* request){

    m_queuelocker.lock();
    // 超出最大数量范围
    if (m_workqueue.size() > m_maxrequestnum){
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}


/**
 * @brief 工作线程入口函数（static），用于 pthread_create 的回调
 * @param arg 传入的 threadpool 对象指针
 * @return 返回传入指针（或者 nullptr）
 */
template <typename T>
void * threadpool<T> :: worker(void *arg){
    threadpool* pool = (threadpool *)arg;
    pool->run();
    return pool;
}


/**
 * @brief 工作线程的实际循环体，等待信号量并从队列中取出任务执行
 * @note 持续运行直到 m_stop 被设置为 true
 */
template <typename T>
void threadpool<T> :: run(){
    while(!m_stop){
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }

        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if (!request) continue;

        request->process();
    }
}


#endif