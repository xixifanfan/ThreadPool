/**
* 项目名称：
*    基于c++17的简易线程池
* 
*/
#include <iostream>
#include<vector>
#include<queue>
#include<future>
#include <queue>
#include <functional>
#include <future>
#include <thread>
#include <chrono>
#include <vector>
#include<cstring>
#include<shared_mutex>
using namespace std;
//资源队列(线程安全)  ===>  queue<T,deque>    
template<class T>
class SafeQue
{
private:
    queue<T>que_;
    mutex mtx_;
public:
    bool isEmpty()noexcept
    {
        //shared_lock<shared_mutex>sharedLock(sharedMtx);//shared_lock开销大于unique_lock
        unique_lock<mutex> _(mtx_);
        return que_.empty();
    }
    size_t size()noexcept
    {
        unique_lock<mutex>_(mtx_);
        return que_.size();
    }
    void push(T& t)noexcept
    {
        unique_lock<mutex>_(mtx_);
        que_.push(t);
    }
    bool pop(T& t)noexcept //获取出队的值或对象
    {
        unique_lock<mutex>_(mtx_);
        if (que_.empty())return false;
        t = move(que_.front());//头部所有权给t，避免拷贝
        que_.pop();
        return true;
    }
};
//线程池
class ThreadPool
{
private:
    using TaskType = function<void()>;
    bool isShutDown_;
    SafeQue<TaskType>que_;
    vector<thread>threads_;
    mutex mtx_;
    condition_variable conditionVar_;
    //执行器
    class Executor
    {
    private:
        ThreadPool* pool_;
    public:
        
        explicit Executor(ThreadPool* pool) :pool_(pool) {}
        void operator()()noexcept
        {
            while (!pool_->isShutDown_)
            {
                //如果线程池未关闭   或   资源队列为空时阻塞，反之运行
                {//用于锁的释放
                    unique_lock<mutex> uniqueLock(pool_->mtx_);
                    pool_->conditionVar_.wait(uniqueLock, [this]{
                        return this->pool_->isShutDown_ || !this->pool_->que_.isEmpty();
                    });
                }
                TaskType fun;
                bool flag = this->pool_->que_.pop(fun);
                if (flag)fun();
            }
        }
    };
public:
    
    explicit ThreadPool(int n) :threads_(n), isShutDown_(false)
    {
        for (auto& t : threads_)t = thread{ Executor(this) };
    }

    //不允许拷贝和赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    //模拟线程传函数入口
    template<class F, class... Args>
    auto exec(F&& f, Args &&...args) -> future<decltype(f(args...))>
    {
        //将函数打包 返回值转换成void
        using ReturnType = typename invoke_result<F,Args...>::type;
        function<ReturnType()>func = bind(forward<F>(f), forward<Args>(args)...); //func封装传入的函数线程入口
        
        auto taskPtr = make_shared<packaged_task<ReturnType()>>(func);//taskPtr指向func
        TaskType warpperFunc = [taskPtr]{//封装成返回值为void的函数 
            (*taskPtr)();
        };
        que_.push(warpperFunc);
        conditionVar_.notify_one();//唤醒线程池中的线程
        return taskPtr->get_future();
    }

    ~ThreadPool()
    {
        //清空资源队列
        
        this->isShutDown_ = true;
        conditionVar_.notify_all();//唤醒所有的线程
        for (auto& t : threads_)//将所有线程回收
        {
            if (t.joinable())t.join();
        }
    }
};
mutex mtx;
int main()
{
    
    ThreadPool pool(8);
    int n = 10;
    auto f1 = [](int id) {
        while (1)
        {
            unique_lock<mutex>lock(mtx);
            cout << "f1:" << id-- << endl;
            if (id == 0)break;
            
        }
        
        };
    auto f2 = [](int id) {
        while (1)
        {
            unique_lock<mutex>lock(mtx);
            cout << "f2:" << id-- << endl;
            if (id == 0)break;
        }
        };
    auto ft1= pool.exec(f1, 10);
    auto ft2 = pool.exec(f2, 100);
    ft1.get();ft2.wait();
    return 0;
}
