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
//��Դ����
template<class T>
class SafeQue
{
private:
    queue<T>que;
    shared_mutex sharedMtx;//c++17�����ԣ����ܸ���mutex�������ڶ����д���
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
    bool pop(T& t)//��ȡ���ӵ�ֵ�����
    {
        unique_lock<shared_mutex>uniqueLock(sharedMtx);
        if (que.empty())return false;
        t = move(que.front());//ͷ������Ȩ��t�����⿽��
        que.pop();
        return true;
    }
};
//�̳߳�
class ThreadPool
{
private:
    class Executor//ִ����
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

    //���������͸�ֵ
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template<class F, class... Args>
    auto exec(F&& f, Args &&...args) -> future<decltype(f(args...))>//ģ���̴߳���
    {
        //��������� ����ֵת����void

        function<decltype(f(args...))()>func = [&f, args...]() {
            return f(args...);
        };//func��װװ���뺯���߳̽ӿ�
        auto taskPtr = make_shared<packaged_task<decltype(f(args...))()>>(func);//taskPtrָ��func
        function<void()>warpperFunc = [taskPtr]{
            (*taskPtr)();
        };//��װ��void 
        que.push(warpperFunc);
        //�����̳߳��е��߳�
        conditionVar.notify_one();
        return taskPtr->get_future();
    }
    ~ThreadPool()
    {
        auto fun = exec([]() {});//��Դ�������
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
