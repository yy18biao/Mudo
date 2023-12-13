#ifndef __M_EVENTLOOP_H__
#define __M_EVENTLOOP_H__

#include "Socket.hpp"
#include <unordered_map>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <sys/eventfd.h>
#include <sys/epoll.h>

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

    // 移除监控
    void Remove();
    // 修改监控
    void Update();

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
        // 可读 || 断开连接 || 优先
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            if (_read_call)
                _read_call();
            // 任意回调所有事件监控都得调用
            // 为了防止活跃度非活跃 则将任意回调用来刷新活跃度
            if (_event_call)
                _event_call();
        }

        // 可写
        if ((_revents & EPOLLOUT))
        {
            if (_write_call)
                _write_call();
            // 任意回调所有事件监控都得调用
            // 为了防止活跃度非活跃 则将任意回调用来刷新活跃度
            if (_event_call)
                _event_call();
        }
        else if ((_revents & EPOLLERR)) // 出错
        {
            // 错误回调调用后连接会断开 因此需要将任意事件回调先调用
            if (_event_call)
                _event_call();
            if (_error_call)
                _error_call();
        }
        else if ((_revents & EPOLLHUP)) // 关闭
        {
            // 关闭回调调用后连接会断开 因此需要将任意事件回调先调用
            if (_event_call)
                _event_call();
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
        if (!IsChannel(channel)){
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

using Func = std::function<void()>;
class EventLoop
{
private:
	std::thread::id _thread_id; // 线程ID
	int _eventfd; // 唤醒IO事件阻塞 事件通知
    std::unique_ptr<Channel> _channel; // 管理eventfd的事件
	Poller _poller; // 使用Poller事件监控
	std::vector<Func> _tasks; // 任务池
	std::mutex _mutex; // 互斥锁
	
private:
	void RunAllTask() // 执行任务池中所有任务
    {
        std::vector<Func> functor;
        {
            std::unique_lock<std::mutex> _lock(_mutex); //加锁
            _tasks.swap(functor);
        }
        // 运行所有任务
        for(auto& func : functor)
            func();
    }

    // 创建eventfd
    static int CreateEventFd()
    {
        int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if(efd < 0)
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
        if(ret < 0)
        {
            // EINTR表示被信号打断 EAGAIN表示没数据可读
            // 这两种情况可以原谅
            if(errno == EINTR || errno == EAGAIN) return;
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
        if(ret < 0)
        {
            if(errno == EINTR) return;
            ERR_LOG("写入eventfd失败");
            abort();
        }
        return;
    }

public:
	EventLoop()
		: _thread_id(std::this_thread::get_id()),
          _eventfd(CreateEventFd()),
          _channel(new Channel(this, _eventfd))
	{
        _channel->SetReadCallBack(std::bind(&EventLoop::ReadEventFd, this));
        _channel->EnableRead();
    }

	// 将要执行的任务判断，处于当前线程直接执行，不是则添加到任务池
	void RunInLoop(const Func& cb)
    {
        if(IsInLoop())
            return cb();
        return InsertTaskLoop(cb);
    }
	
	// 将任务加入任务池
	void InsertTaskLoop(const Func& cb)
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
	void UpdateEvent(Channel* channel)
    {
        return _poller.UpdateEvent(channel);
    } 

    // 移除描述符监控
	void RemoveEvent(Channel* channel)
    {
        return _poller.RemoveEvent(channel);
    }
	
	// EventLoop启动功能
	void Start()
    {
        // 事件监控
        std::vector<Channel*> actives;
        _poller.Poll(&actives);

        // 事件处理
        for(auto& channel : actives)
            channel->HandleEvent();

        // 执行任务
        RunAllTask();
    }
};

// 由于EventLoop类在Channel类之后定义
// 这两接口又使用到EventLoop类的接口
// 因此需要拿到EventLoop类之后去定义
void Channel::Remove() { _eventloop->RemoveEvent(this); }
void Channel::Update() { _eventloop->UpdateEvent(this); }

#endif