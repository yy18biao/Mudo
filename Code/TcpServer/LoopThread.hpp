#ifndef __M_LOOPTHREAD_H__
#define __M_LOOPTHREAD_H__

#include "EventLoop.hpp"
#include <condition_variable>

class LoopThread
{
private:
    std::mutex _mutex;             // 互斥锁
    std::condition_variable _cond; // 条件变量
    EventLoop *_loop;              // 这个指针对象需要在线程内实例化
    std::thread _thread;           // 对应线程

private:
    // 线程入口函数 主要用来实例化EventLOop对象
    // 并开启EventLoop模块功能
    void ThreadEntry()
    {
        // 不使用new实例化是为了这个loop能随着
        // 该LoopThread对象的生命周期同时销毁
        EventLoop loop;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _loop = &loop;
            _cond.notify_all(); // 唤醒其他线程
        }
        // 启动loop功能
        loop.Start();
    }

public:
    LoopThread()
        : _loop(nullptr), _thread(std::thread(&LoopThread::ThreadEntry, this)) {}

    // 获取当前线程所关联的EventLoop对象指针
    EventLoop *GetLoop()
    {
        EventLoop *loop = nullptr;
        {
            // 使用条件变量和加锁为了防止线程创建了但是loop还没有实例化之前就获取loop
            // 这样获取的loop就是一个空值
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [&]
                       { return _loop != nullptr; });
            loop = _loop;
        }
        return loop;
    }
};

class LoopThreadPool
{
private:
    int _count;                         // 从属线程数量
    int _loop_idx;                      // loops的偏移量
    EventLoop *_loop;                   // 运行在主线程的EventLoop
    std::vector<LoopThread *> _threads; // 保存所有LoopThread对象
    std::vector<EventLoop *> _loops;    // 保存所有的EventLoop对象

public:
    LoopThreadPool(EventLoop *loop) : _count(0), _loop_idx(0), _loop(loop) {}
    // 设置从属线程数量
    void SetCount(int count) { _count = count; }
    // 创建所有的从属线程
    void Create()
    {
        if (_count == 0)
            return;
        _threads.resize(_count);
        _loops.resize(_count);
        for (int i = 0; i < _count; ++i)
        {
            _threads[i] = new LoopThread();
            _loops[i] = _threads[i]->GetLoop();
        }
    }
    // 获取下一个EventLoop
    EventLoop *GetNextLoop()
    {
        // 如果从属线程数量为0 则只需要返回主线程的EventLoop也就是_loop即可
        if (_count == 0)
            return _loop;
        _loop_idx = (_loop_idx + 1) % _count;
        return _loops[_loop_idx];
    }
};

#endif