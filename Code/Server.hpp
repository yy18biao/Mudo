#ifndef __M_SERVER_H__
#define __M_SERVER_H__

#include "Connection.hpp"
#include "Acceptor.hpp"
#include "LoopThread.hpp"
#include <signal.h>

class Server
{
private:
    uint64_t _id;                                       // 自增长
    int _port;                                          // 端口号
    int _timenum;                                       // 多长时间无通信就是非活跃连接
    bool _active_sign;                                  // 是否启动非活跃销毁，默认false
    EventLoop _loop;                                    // 主线程的EventLoop对象，负责监听事件的处理
    Acceptor _acceptor;                                 // 监听套接字的管理对象
    std::unordered_map<uint64_t, ConnectionPtr> _conns; // 管理所有Connection的shared_ptr对象
    LoopThreadPool _loop_pool;                          // 从属EventLoop线程池
    /* 连接的回调函数类型 用户使用设置 */
    using ConnectedCallBack = std::function<void(const ConnectionPtr &)>;
    using MessageCallBack = std::function<void(const ConnectionPtr &, Buffer *)>;
    using CloseCallBack = std::function<void(const ConnectionPtr &)>;
    using AnyEventCallBack = std::function<void(const ConnectionPtr &)>;
    ConnectedCallBack _conn_callback;
    MessageCallBack _mess_callback;
    CloseCallBack _close_callback;
    AnyEventCallBack _event_callback;

private:
    // 为新连接构造Connection进行管理
    void CreateConnForNew(int fd)
    {
        ++_id;
        ConnectionPtr conn(new Connection(_loop_pool.GetNextLoop(), _id, fd));
        conn->SetMessageCall(_mess_callback);
        conn->SetConnectedCall(_conn_callback);
        conn->SetCloseCall(_close_callback);
        conn->SetAnyEventCall(_event_callback);
        conn->SetServerCloseCall(std::bind(&Server::RemoveConnInLoop, this, std::placeholders::_1));
        if (_active_sign)
            conn->EnableActive(_timenum);
        conn->SetUp();
        _conns.insert(std::make_pair(_id, conn));
    }

    // 从_conns中移除连接信息在对应EventLoop中执行
    void RemoveConnInLoop(const ConnectionPtr &conn)
    {
        int id = conn->GetConnId();
        auto it = _conns.find(id);
        if (it != _conns.end())
            _conns.erase(it);
    }

    // 添加定时任务在对应EventLoop中执行
    void AddTimedTaskInLoop(const Func &task, int delay)
    {
        ++_id;
        _loop.TimerAdd(_id, delay, task);
    }

public:
    Server(int port) : _port(port), _id(0), _timenum(0), _active_sign(false), _acceptor(&_loop, _port), _loop_pool(&_loop)
    {
        _acceptor.SetAcceptCallBack(std::bind(&Server::CreateConnForNew, this, std::placeholders::_1));
        _acceptor.Listen();
    }

    // 设置线程池数量
    void SetThreadPoolCount(int count) { _loop_pool.SetCount(count); }
    /* 设置回调函数 */
    void SetConnectedCall(const ConnectedCallBack &cb) { _conn_callback = cb; }
    void SetMessageCall(const MessageCallBack &cb) { _mess_callback = cb; }
    void SetCloseCall(const CloseCallBack &cb) { _close_callback = cb; }
    void SetAnyEventCall(const AnyEventCallBack &cb) { _event_callback = cb; }
    // 启动非活跃销毁
    bool EnableActive(int timenum)
    {
        _timenum = timenum;
        _active_sign = true;
    }
    // 添加定时任务
    void AddTimedTask(const Func &task, int delay)
    {
        _loop.RunInLoop(std::bind(&Server::AddTimedTaskInLoop, this, task, delay));
    }
    // 启动服务器
    void Start()
    {
        _loop_pool.Create();
        _loop.Start();
    }
};


class NetWork {
    public:
        NetWork() {
            DBG_LOG("SIGPIPE INIT");
            signal(SIGPIPE, SIG_IGN);
        }
};
static NetWork nw;

#endif
