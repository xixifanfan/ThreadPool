/**
* ��Ŀ���ƣ�
*    ����c++17�ļ����̳߳�
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
//��Դ����(�̰߳�ȫ)  ===>  queue<T,deque>    
template<class T>
class SafeQue
{
private:
    queue<T>que;
    shared_mutex sharedMtx;//c++17�����ܸ���mutex�������ڶ����д���
public:
    bool isEmpty()const noexcept
    {
        //shared_lock<shared_mutex>sharedLock(sharedMtx);//shared_lock��������unique_lock
        unique_lock<sharedMtx>uniqueLock(sharedMtx);
        return que.empty();
    }
    size_t size() const noexcept
    {
        unique_lock<sharedMtx>uniqueLock(sharedMtx);
        return que.size();
    }
    void push(T& t)noexcept
    {
        unique_lock<shared_mutex>uniqueLock(sharedMtx);
        que.push(t);
    }
    bool pop(T& t)noexcept //��ȡ���ӵ�ֵ�����
    {
        if (que.empty())return false;
        unique_lock<shared_mutex>uniqueLock(sharedMtx);
        t = move(que.front());//ͷ������Ȩ��t�����⿽��
        que.pop();
        return true;
    }
};
//�̳߳�
class ThreadPool
{
private:
    //ִ����
    class Executor
    {
    public:
        ThreadPool* pool;
        explicit Executor(ThreadPool* pool) :pool(pool) {}
        void operator()()
        {
            while (!pool->isShutDown)
            {
                //����̳߳عر�   ��   ��Դ����Ϊ��ʱ��������֮����
                {//���������ͷ�
                    unique_lock<mutex> uniqueLock(pool->mtx);
                    pool->conditionVar.wait(uniqueLock, [this]{
                        return this->pool->isShutDown || !this->pool->que.isEmpty();
                    });
                }
                function<void()>fun;
                bool flag = this->pool->que.pop(fun);
                if (flag)fun();
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

    //���������͸�ֵ
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    //ģ���̴߳��������
    template<class F, class... Args>
    auto exec(F&& f, Args &&...args) -> future<decltype(f(args...))>
    {
        //��������� ����ֵת����void

        function<decltype(f(args...))()>func = [&f, args...]() {//func��װ����ĺ����߳����
            return f(args...);
        };
        auto taskPtr = make_shared<packaged_task<decltype(f(args...))()>>(func);//taskPtrָ��func
        function<void()>warpperFunc = [taskPtr]{//��װ�ɷ���ֵΪvoid�ĺ��� 
            (*taskPtr)();
        };
        que.push(warpperFunc);
        conditionVar.notify_one();//�����̳߳��е��߳�
        return taskPtr->get_future();
    }
    ~ThreadPool()
    {
        //�����Դ����
        auto fun = exec([]() {});
        fun.get();
        this->isShutDown = true;
        conditionVar.notify_all();//�������е��߳�
        for (auto& t : threads)//�������̻߳���
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
