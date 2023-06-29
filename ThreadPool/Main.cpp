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
//资源队列
template<class T>
class SafeQue
{
private:
    queue<T>que;
    shared_mutex sharedMtx;//c++17新特性，性能高于mutex，常用于多读少写情况
public:
    bool isEmpty()
    {
        shared_lock<shared_mutex>sharedLock(sharedMtx);
        return que.empty();
    }
    size_t size()
    {
        shared_lock<shared_mutex>sharedLock(sharedMtx);
        return que.size();
    }
    void push(T& t)
    {
        unique_lock<shared_mutex>uniqueLock(sharedMtx);
        que.push(t);
    }
    bool pop(T& t)//获取出队的值或对象
    {
        unique_lock<shared_mutex>uniqueLock(sharedMtx);
        if (que.empty())return false;
        t = move(que.front());//头部所有权给t，避免拷贝
        que.pop();
        return true;
    }
};
//线程池
class ThreadPool
{
private:
    class Executor//执行者
    {
    public:
        ThreadPool* pool;
        explicit Executor(ThreadPool* pool) :pool(pool) {}
        void operator()()
        {
            while (!pool->isShutDown)
            {
                //如果线程池关闭   或   资源队列为空时阻塞，反之运行
                {//用于锁的释放
                    unique_lock<mutex> uniqueLock(pool->mtx);
                    pool->conditionVar.wait(uniqueLock, [this]{
                        return this->pool->isShutDown || !this->pool->que.isEmpty();
                    });
                }
                function<void()>fun;
                bool flag = this->pool->que.pop(fun);
                if (flag)
                {
                    fun();
                }
            }
        }
    };
    bool isShutDown;
    SafeQue<function<void()>>que;
    vector<thread>threads;
    mutex mtx;
    condition_variable conditionVar;
public:
    
    explicit ThreadPool(int n) :threads(n), isShutDown(false)
    {
        for (auto& t : threads)t = thread{ Executor(this) };
    }

    //不允许拷贝和赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template<class F, class... Args>
    auto exec(F&& f, Args &&...args) -> future<decltype(f(args...))>//模拟线程传参
    {
        //将函数打包 返回值转换成void

        function<decltype(f(args...))()>func = [&f, args...]() {
            return f(args...);
        };//func封装装传入函数线程接口
        auto taskPtr = make_shared<packaged_task<decltype(f(args...))()>>(func);//taskPtr指向func
        function<void()>warpperFunc = [taskPtr]{
            (*taskPtr)();
        };//封装成void 
        que.push(warpperFunc);
        //唤醒线程池中的线程
        conditionVar.notify_one();
        return taskPtr->get_future();
    }
    ~ThreadPool()
    {
        auto fun = exec([]() {});//资源队列清空
        fun.get();
        this->isShutDown = true;
        conditionVar.notify_all();//唤醒所有的线程
        for (auto& t : threads)//将所有线程回收
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
    for (int i = 1;i <= n;i++)
    {
        pool.exec([](int id) {
            if (id % 2 == 0)
            {
                this_thread::sleep_for(2s);
            }
            unique_lock<mutex>uniqueLock(mtx);
            cout << "id : " << id << endl;
        }, i);
    }
    return 0;
}
