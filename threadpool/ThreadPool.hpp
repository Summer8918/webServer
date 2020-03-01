#include<iostream>
#include<vector>
#include<queue>
#include<string>
#include <future> //std::future
#include<thread> //this_thread::sleep_for
#include <chrono> //std::chrono::seconds
#include<functional> //std::bind
class ThreadPool{
public:
    //在线程池中创建threads个工作线程
    ThreadPool(size_t threads);

    //向线程池中增加线程
    template<typename F,typename... Args>
        auto enqueue(F&& f,Args&&... args)
       -> std::future<typename std::result_of<F(Args...)>::type>;

    ~ThreadPool();
private:
    //需要持续追踪线程来保证可以使用join
    std::vector<std::thread> workers;
    //任务队列
    std::queue<std::function<void()>> tasks;

    //互斥锁
    std::mutex queue_mutex;  
    std::condition_variable condition;
    //停止相关
    bool stop;
};

 /*构造函数
 * 构造的线程池指定可并发执行的线程的数量
 * 任务的执行和结束阶段位于临界区，保证不会并发的同时启动多个需要执行的任务
 */

inline ThreadPool::ThreadPool(size_t threads):stop(false){
    //启动threads数量的工作线程worker
    for(size_t i=0;i<threads;++i){
        std::cout<<"threads i="<<i<<std::endl;
        workers.emplace_back(
                //此处的lambda表达式捕获this，即线程池实例
                [this]{
                //循环避免虚假唤醒
                for(;;){
                //定义函数对象的容器，存储任意的返回类型为void，参数表为空的函数
                    std::function<void()> task;
                    //临界区
                    {
                        //创建互斥锁
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
        
                        //阻塞当前线程，直到condition_variable被唤醒 等待lambda表达式返回true
                        this->condition.wait(lock,
                                [this]{ return this->stop || !this->tasks.empty();});

                        if(this->stop && this->tasks.empty()){
                            return;
                        }

                        //否则就让任务队列的队首任务作为需要执行的任务出队
                        std::cout<<"start handle task"<<std::endl;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    //执行当前任务
                    task();
                }
                }
        );
    }
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
    for(std::thread &worker:workers)
        worker.join();
}

/*
 * 向线程池中添加新任务
 * 支持多个入队任务参数需要使用变长模板参数
 * RAII “资源获取即初始化方式”(RAII，Resource Acquisition Is Initialization)
 * */

template<class F,class... Args>
auto ThreadPool::enqueue(F &&f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    //推导任务返回类型
    using return_type = typename std::result_of<F(Args...)>::type;

    //获得当前任务
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f),std::forward<Args>(args)...)
            );

    //获得std::future对象以供实施线程同步
    std::future<return_type> res = task->get_future();
    
    //临界区
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        //禁止在线程池停止后加入新的线程
        if(stop){
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        //将线程添加到执行任务队列中
        tasks.emplace([task]{ (*task)();});
    }

    //通知一个正在等待的线程
    condition.notify_one();
    return res;
}
