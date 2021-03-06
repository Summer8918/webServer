#include "threadpool.h"

int main()
{
    ThreadPool pool(2,4); //创建一个能够并发执行4个线程的线程池
    
    std::vector<std::future<std::string>> results;

    //启动8个需要执行的线程任务
    for(int i=0;i<8;i++){
        //将并发执行任务添加到线程池中并发执行
        //emplace_back()与push_back()的功能类似
        //用于将元素添加到vector尾部
        //emplace_back()省略了进行构造函数的过程
        //就地构造，没有复制或移动操作进行
        results.emplace_back(
                pool.enqueue([i]{   //lambda表达式
                std::cout<<"hello "<<i<<std::endl;
                //上一行输出后，该线程会等待1秒钟
                std::this_thread::sleep_for(std::chrono::seconds(1));
                //然后再继续输出并返回执行情况
                std::cout<<"world "<<i<<std::endl;
                return std::string("---thread ")+std::to_string(i)+std::string(" finished.---");
                })
                );
    }
    //输出线程任务的结果
    for(auto && result:results){
        std::cout<<result.get()<<' ';
    }
    std::cout<<std::endl;
    return 0;
}

