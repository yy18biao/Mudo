#ifndef __M_LOOPTHREAD_H__
#define __M_LOOPTHREAD_H__

#include "EventLoop.hpp"
#include <condition_variable>

class LoopThread
{
private:
    std::mutex _mutex; // 互斥锁
    std::condition_variable _cond; // 条件变量
    EventLoop *_loop;  // 这个指针对象需要在线程内实例化
    std::thread _thread; // 对应线程
    
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
    EventLoop* GetLoop()
    {
        EventLoop *loop = nullptr;
        {
            // 使用条件变量和加锁为了防止线程创建了但是loop还没有实例化之前就获取loop
            // 这样获取的loop就是一个空值
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [&]{ return _loop != nullptr; });
            loop = _loop;
        }
        return loop;
    }
};

#endif