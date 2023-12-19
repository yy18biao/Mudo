#ifndef __M_ACCEPTOR_H__
#define __M_ACCEPTOR_H__

#include "Socket.hpp"
#include "EventLoop.hpp"

using AcceptCallBack = std::function<void(int)>; // 获取新连接成功后的回调函数
class Acceptor
{
private:
    Socket _socket; // 用于创建监听套接字
    EventLoop* _loop; // 对监听套接字进行监控
    Channel _channel; // 对监听套接字进行事件管理
    AcceptCallBack _callback;

private:
    // 读事件回调
    void HandleRead()
    {
        int newfd = _socket.Accept();
        if(newfd < 0) return;
        if(_callback) _callback(newfd);
    }
    // 创建服务器连接
    int CreateServer(int port)
    {
        assert(_socket.CreateServer(port) == true);
        return _socket.GetFd();
    }

public:
    // 不能将启动读事件监控放到构造函数中
    // 防止启动监控后立即有事件但是回调函数还未设置
    Acceptor(EventLoop* loop, int port)
        : _socket(CreateServer(port)), _loop(loop), _channel(loop, _socket.GetFd())
    {
        _channel.SetReadCallBack(std::bind(&Acceptor::HandleRead, this));
    }
    // 设置获取新连接成功后的回调函数
    void SetAcceptCallBack(const AcceptCallBack& cb) { _callback = cb; } 

    // 启动读事件监控
    void Listen() { _channel.EnableRead(); }
};

#endif