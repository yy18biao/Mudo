#ifndef __M_EVENTLOOP_H__
#define __M_EVENTLOOP_H__

#include "Socket.hpp"
#include <unordered_map>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <vector>
#include <cstring>
#include <cassert>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

using EventCallBack = std::function<void()>;
class EventLoop;
class Channel
{
private:
    int _fd;
    EventLoop *_eventloop;
    uint32_t _events;          // 当前需要监控的事件
    uint32_t _revents;         // 当前连接触发的事件
    EventCallBack _read_call;  // 可读事件被触发回调函数
    EventCallBack _write_call; // 可写事件被触发回调函数
    EventCallBack _error_call; // 错误事件被触发回调函数
    EventCallBack _close_call; // 连接断开事件被触发回调函数
    EventCallBack _event_call; // 任意事件被触发回调函数

public:
    Channel(EventLoop *eventloop, int fd) : _fd(fd), _eventloop(eventloop), _events(0), _revents(0) {}
    int GetFd() { return _fd; }
    uint32_t GetEvent() { return _events; }

public:
    void SetREvents(uint32_t events) { _revents = events; }

    /* 设置各个回调函数 */
    void SetReadCallBack(const EventCallBack &cb)
    {
        _read_call = cb;
    }
    void SetWriteCallBack(const EventCallBack &cb)
    {
        _write_call = cb;
    }
    void SetErrorCallBack(const EventCallBack &cb)
    {
        _error_call = cb;
    }
    void SetCloseCallBack(const EventCallBack &cb)
    {
        _close_call = cb;
    }
    void SetEventCallBack(const EventCallBack &cb)
    {
        _event_call = cb;
    }

    // 判断当前是否可读
    bool ReadAble() { return (_events & EPOLLIN); }

    // 判断当前是否可写
    bool WritrAble() { return (_events & EPOLLOUT); }

    /*
        由于使用了EventLoop类的方法
        所以定义在EventLoop类下
    */
    void Remove(); // 移除监控
    void Update(); // 修改监控

    // 启动可读事件监控
    void EnableRead()
    {
        _events |= EPOLLIN;
        Update();
    }

    // 启动可写事件监控
    void EnableWrite()
    {
        _events |= EPOLLOUT;
        Update();
    }

    // 关闭可读事件监控
    void DisableRead()
    {
        _events &= ~EPOLLIN;
        Update();
    }

    // 关闭可写事件监控
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        Update();
    }

    // 关闭所有事件监控
    void DisableAll()
    {
        _events = 0;
        Update();
    }

    // 一旦连接触发了事件就调用这个函数
    // 决定了触发了什么事件应该调用哪个函数
    void HandleEvent()
    {
        if (_event_call) // 任意
            _event_call();
        // 可读 || 断开连接 || 优先
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
            if (_read_call)
                _read_call();

        if (_revents & EPOLLOUT) // 可写
        {
            if (_write_call)
                _write_call();
        }
        else if (_revents & EPOLLERR) // 出错
        {
            if (_error_call)
                _error_call(); // 一旦出错，就会释放连接，因此要放到前边调用任意回调
        }
        else if (_revents & EPOLLHUP) // 断开连接
        {
            if (_close_call)
                _close_call();
        }
    }
};

#define EVENTSIZE 1024
class Poller
{
private:
    int _epfd;                                    // epoll操作句柄
    struct epoll_event _evs[EVENTSIZE];           // epoll_event数组 监控活跃事件
    std::unordered_map<int, Channel *> _channels; // hash表管理描述符和Channel对象

private:
    // 对epoll直接操作
    void Update(Channel *channel, int op)
    {
        // int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
        struct epoll_event ev;
        ev.data.fd = channel->GetFd();
        ev.events = channel->GetEvent();
        if (epoll_ctl(_epfd, op, channel->GetFd(), &ev) < 0)
        {
            ERR_LOG("操作epoll出错");
            abort();
        }
    }

    // 判断一个Channel是否已经被管理
    bool IsChannel(Channel *channel)
    {
        if (_channels.find(channel->GetFd()) == _channels.end())
            return false;
        return true;
    }

public:
    Poller()
    {
        _epfd = epoll_create(EVENTSIZE);
        if (_epfd < 0)
        {
            ERR_LOG("创建epoll句柄失败");
            abort();
        }
    }

    // 添加或修改描述符事件监控
    void UpdateEvent(Channel *channel)
    {
        // 不存在则添加 存在则修改
        if (!IsChannel(channel))
        {
            _channels.insert(std::make_pair(channel->GetFd(), channel));
            Update(channel, EPOLL_CTL_ADD);
        }
        else
            Update(channel, EPOLL_CTL_MOD);
    }

    // 移除描述符事件监控
    void RemoveEvent(Channel *channel)
    {
        if (_channels.find(channel->GetFd()) != _channels.end())
            _channels.erase(channel->GetFd());

        Update(channel, EPOLL_CTL_DEL);
    }

    // 开始监控 获取活跃就绪Channel
    void Poll(std::vector<Channel *> *channels)
    {
        // int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
        int ret = epoll_wait(_epfd, _evs, EVENTSIZE, -1);
        if (ret < 0)
        {
            if (errno == EINTR)
                return;
            ERR_LOG("epoll监控失败：%s\n", strerror(errno));
            abort();
        }
        for (int i = 0; i < ret; ++i)
        {
            auto it = _channels.find(_evs[i].data.fd);
            assert(it != _channels.end());
            // 设置实际就绪的事件
            it->second->SetREvents(_evs[i].events);
            channels->push_back(it->second);
        }
    }
};

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
class TimerTask // 定时器任务类
{
private:
    uint64_t _id;         // 定时器任务对象ID
    uint32_t _timeout;    // 定时任务的超时时间
    bool _canceled;       // 是否要取消任务的标志
    TaskFunc _task;       // 定时器对象要执行的任务
    ReleaseFunc _release; // 用于删除TimerWheel中保存的定时器对象信息

public:
    TimerTask(uint64_t id, uint32_t timeout, const TaskFunc &cb)
        : _id(id), _timeout(timeout), _task(cb), _canceled(false)
    {
    }

    // 对象被销毁时执行任务
    ~TimerTask()
    {
        if (_canceled == false)
            _task();
        _release();
    }

public:
    void SetRelease(const ReleaseFunc &cb)
    {
        _release = cb;
    }

    // 取消任务
    void Cancel() { _canceled = true; }

    uint32_t GetTimeOut() { return _timeout; }
};

using TaskPtr = std::shared_ptr<TimerTask>;
using WeakPtrTask = std::weak_ptr<TimerTask>;
class EventLoop;
class TimerWheel
{
private:
    int _tick;     // 当前的秒针
    int _capacity; // 数组最大容量也就是最大延迟时间
    std::vector<std::vector<TaskPtr>> _wheel;
    std::unordered_map<uint64_t, WeakPtrTask> _timers;
    EventLoop *_loop;
    int _timerfd; // 定时器描述符
    std::unique_ptr<Channel> _timer_channel;

private:
    void RemoveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
            _timers.erase(it);
    }

    // 创建Timer文件描述符
    static int CreateTimerFd()
    {
        int fd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (fd < 0)
        {
            ERR_LOG("创建定时器描述符失败");
            abort();
        }
        struct itimerspec it;
        // 设置第一次超时时间
        it.it_value.tv_sec = 1;
        it.it_value.tv_nsec = 0;
        // 设置第⼀次之后的超时间隔时间
        it.it_interval.tv_sec = 1;
        it.it_interval.tv_nsec = 0;
        // 启动
        timerfd_settime(fd, 0, &it, nullptr);
        return fd;
    }

    // 读取定时器
    int ReadTimerFd()
    {
        uint64_t times;
        int ret = read(_timerfd, &times, 8);
        if (ret < 0)
        {
            ERR_LOG("读取定时器失败");
            abort();
        }
        return times;
    }

    // 执行定时任务
    void run()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear();
    }

    // 时间到了之后读取定时器并执行定时任务
    void OnTime()
    {
        // 根据实际超时次数执行对应次数的超时任务
        int times = ReadTimerFd();
        for(int i = 0; i < times; ++i)
            run();
    }

    // 添加定时任务
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb)
    {
        TaskPtr pt(new TimerTask(id, delay, cb));
        pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
        _timers[id] = WeakPtrTask(pt);
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
    }

    // 延迟定时任务
    void TimerRefreshInLoop(uint64_t id)
    {
        // 通过保存的定时器对象的weak_ptr构造shared_ptr出来，添加到轮子
        auto it = _timers.find(id);
        if (it == _timers.end())
            return;

        // 获取定时器对象的weak_ptr构造shared_ptr对象
        TaskPtr pt = it->second.lock();
        int delay = pt->GetTimeOut();
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
    }

    // 取消定时任务
    void TimerCancelInLoop(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
            return;

        TaskPtr pt = it->second.lock();
        if (pt)
            pt->Cancel();
    }

public:
    TimerWheel(EventLoop *loop)
        : _loop(loop), _capacity(60), _tick(0), _wheel(_capacity), _timerfd(CreateTimerFd()), _timer_channel(new Channel(_loop, _timerfd))
    {
        _timer_channel->SetReadCallBack(std::bind(&TimerWheel::OnTime, this));
        _timer_channel->EnableRead();
    }

    /*
        考虑到线程安全则需要用EventLoop去判断
        如果是该线程的任务则直接执行 否则压入EventLoop的任务池
        由于使用了EventLoop类的方法
        所以定义在EventLoop类下
    */
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb); // 添加定时任务
    void TimerRefresh(uint64_t id);                                 // 刷新定时任务
    void TimerCancel(uint64_t id);                                  // 取消定时任务

    // 判断是否存在某任务
    // 存在线程安全 因此不能再外部使用
    // 只能在对应的EventLoop线程内执行
    bool HasTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
            return false;
        return true;
    }
};

using Func = std::function<void()>;
class EventLoop
{
private:
    std::thread::id _thread_id;        // 线程ID
    int _eventfd;                      // 唤醒IO事件阻塞 事件通知
    std::unique_ptr<Channel> _channel; // 管理eventfd的事件
    Poller _poller;                    // 使用Poller事件监控
    std::vector<Func> _tasks;          // 任务池
    std::mutex _mutex;                 // 互斥锁
    TimerWheel _timer_wheel;           // 定时器

private:
    void RunAllTask() // 执行任务池中所有任务
    {
        std::vector<Func> functor;
        {
            std::unique_lock<std::mutex> _lock(_mutex); // 加锁
            _tasks.swap(functor);
        }
        // 运行所有任务
        for (auto &func : functor)
            func();
    }

    // 创建eventfd
    static int CreateEventFd()
    {
        int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (efd < 0)
        {
            ERR_LOG("创建eventfd失败");
            abort();
        }
        return efd;
    }

    // 读取eventfd
    void ReadEventFd()
    {
        uint64_t res = 0;
        int ret = read(_eventfd, &res, sizeof res);
        if (ret < 0)
        {
            // EINTR表示被信号打断 EAGAIN表示没数据可读
            // 这两种情况可以原谅
            if (errno == EINTR || errno == EAGAIN)
                return;
            ERR_LOG("读取eventfd失败");
            abort();
        }
        return;
    }

    // 写入eventfd用于唤醒阻塞线程
    void WeakUpEventFd()
    {
        uint64_t val = 1;
        int ret = write(_eventfd, &val, sizeof val);
        if (ret < 0)
        {
            if (errno == EINTR)
                return;
            ERR_LOG("写入eventfd失败");
            abort();
        }
        return;
    }

public:
    EventLoop()
        : _thread_id(std::this_thread::get_id()),
          _eventfd(CreateEventFd()),
          _channel(new Channel(this, _eventfd)),
          _timer_wheel(this)
    {
        _channel->SetReadCallBack(std::bind(&EventLoop::ReadEventFd, this));
        _channel->EnableRead();
    }

    // 将要执行的任务判断，处于当前线程直接执行，不是则添加到任务池
    void RunInLoop(const Func &cb)
    {
        if (IsInLoop())
            return cb();
        return InsertTaskLoop(cb);
    }

    // 将任务加入任务池
    void InsertTaskLoop(const Func &cb)
    {
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.push_back(cb);
        }

        // 线程有可能没有事件就绪导致阻塞
        // 因此压入任务后就唤醒一次阻塞线程
        // 本质就是给eventfd写入一个数据让其触发一个可读事件
        WeakUpEventFd();
    }

    // 判断当前线程是否是EventLoop对应的线程
    bool IsInLoop()
    {
        return _thread_id == std::this_thread::get_id();
    }

    // 添加/修改描述符监控事件
    void UpdateEvent(Channel *channel)
    {
        return _poller.UpdateEvent(channel);
    }

    // 移除描述符监控
    void RemoveEvent(Channel *channel)
    {
        return _poller.RemoveEvent(channel);
    }

    // EventLoop启动功能
    void Start()
    {
        while (1)
        { 
            // 事件监控
            std::vector<Channel *> actives;
            _poller.Poll(&actives);

            // 事件处理
            for (auto &channel : actives)
                channel->HandleEvent();

            // 执行任务
            RunAllTask();
        }
    }

    // 添加定时器任务
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb)
    {
        return _timer_wheel.TimerAdd(id, delay, cb);
    }

    // 刷新定时器任务
    void TimerRefersh(uint64_t id)
    {
        return _timer_wheel.TimerRefresh(id);
    }

    // 取消定时任务
    void TimerCancel(uint64_t id)
    {
        return _timer_wheel.TimerCancel(id);
    }

    // 判断定时任务是否存在
    bool HasTimer(uint64_t id)
    {
        return _timer_wheel.HasTimer(id);
    }
};

// 由于EventLoop类在Channel类和TimerWheel类之后定义
// 这两接口又使用到EventLoop类的接口
// 因此需要拿到EventLoop类之后去定义
void Channel::Remove() { _eventloop->RemoveEvent(this); }
void Channel::Update() { _eventloop->UpdateEvent(this); }
void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb)
{
    return _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}
void TimerWheel::TimerRefresh(uint64_t id)
{
    return _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}
void TimerWheel::TimerCancel(uint64_t id)
{
    return _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}

#endif