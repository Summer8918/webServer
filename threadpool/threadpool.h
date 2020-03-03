#pragma once
#ifndef THREAD_POOL_HPP
#include<iostream>
#include<vector>
#include<queue>
#include<string>
#include <future> //std::future
#include<thread> //this_thread::sleep_for
#include <chrono> //std::chrono::seconds
#include<functional> //std::bind
#include<atomic>

class ThreadPool{
public:
    //在线程池中创建threads个工作线程
    ThreadPool(size_t threads,size_t max_threads_nums);

    //向线程池中增加线程
    template<typename F,typename... Args>
        auto enqueue(F&& f,Args&&... args)
       -> std::future<typename std::result_of<F(Args...)>::type>;
    //尾置返回类型
    void addThread(size_t size);
    ~ThreadPool();
public:
    std::atomic<int> THREADPOOL_MAX_NUM {4};
    //需要持续追踪线程来保证可以使用join
    std::vector<std::thread> workers;//线程池
    //任务队列
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;  
    std::condition_variable condition;
    //停止相关
    bool stop;
    std::atomic<int>  _idlThrNum{ 0 };  //空闲线程数量
    int idlCount() {
        return _idlThrNum;
    }
    int thrCount(){
        return workers.size();
    }
};

 /*
  * 添加指定数量的线程
 */

void ThreadPool::addThread(size_t size){
    //增加线程数量，但不超过预定义数量 THREADPOOL_MAX_NUM
    for(;workers.size() < THREADPOOL_MAX_NUM && size >0;--size){
        workers.emplace_back(   //工作线程函数
                [this]{
                for(;;){
                    //临界区
                    std::function<void()> task;  //获取一个待执行的task
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,[this]{return stop || !this->tasks.empty();});
                        if(this->stop && tasks.empty()){
                            return;
                        }
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    _idlThrNum--;
                    //执行当前任务
                    task();
                    _idlThrNum++;
                }
                }
            );
        _idlThrNum++;
    }
}

inline ThreadPool::ThreadPool(size_t threads,size_t max_threads_nums):stop(false){
    THREADPOOL_MAX_NUM=max_threads_nums;
    addThread(threads);
}

/*
 * 线程池的销毁对应构造时究竟创建了什么实例
 * 在销毁线程池之前，我们不知道当前线程池中的工作线程是否执行完成，所以必须先创建一个临界区
 * 将线程池状态标记为停止，从而禁止新的线程的加入，最后等待所有执行线程的运行结束，完成销毁
 * */

inline ThreadPool::~ThreadPool(){
    //临界区
    {  
        //创建互斥锁
        std::unique_lock<std::mutex> lock(queue_mutex);

        //设置线程池状态
        stop=true;
    }

    //通知所有等待线程
    condition.notify_all();

    //使所有异步线程转为同步执行
    for(std::thread &worker:workers){
        if(worker.joinable()){
            worker.join();  //等待任务结束
        }
    }
}

/*
 * 向线程池中添加新任务
 * 支持多个入队任务参数需要使用变长模板参数
 * 为了调度执行的任务，需要包装执行的任务，这就意味着需要对任务函数的类型进行包装、构造
 * 临界区可以在一个作用域里边被创建，最佳实践是使用RAII的形式
 * RAII “资源获取即初始化方式”(RAII，Resource Acquisition Is Initialization)
 * */

template<class F,class... Args>
auto ThreadPool::enqueue(F &&f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    if(stop){
        throw std::runtime_error("enqueue on threadpool is stopped.");
    }
    //推导任务返回类型
    using return_type = typename std::result_of<F(Args...)>::type;

    //获得当前任务，把函数入口及参数打包绑定
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f),std::forward<Args>(args)...)
            );

    //获得std::future对象以供实施线程同步
    std::future<return_type> res = task->get_future();
    
    //临界区,添加任务到队列
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        //禁止在线程池停止后加入新的线程
        if(stop){
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        //将线程添加到执行任务队列中
        tasks.emplace([task]{ (*task)();});
    }

    if(_idlThrNum < 1 && workers.size() < THREADPOOL_MAX_NUM){
        addThread(1);
    }
    //通知一个正在等待的线程
    condition.notify_one();
    return res;
}

#endif // THREAD_POOL_HPP
