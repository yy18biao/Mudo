#ifndef __M_CONNECTION_H__
#define __M_CONNECTION_H__

#include "EventLoop.hpp"
#include "Socket.hpp"
#include "Buffer.hpp"

/* 连接的状态 */
typedef enum
{
    DISCONECTED,  // 连接关闭状态
    CONNECTING,   // 连接建立成功待处理状态
    CONNECTED,    // 连接建立完成可进行操作状态
    DISCONNECTING // 连接关闭状态
} ConnStatu;

using ConnectionPtr = std::shared_ptr<Connection>; // Connection智能指针

class Connection : public std::enable_shared_from_this<Connection>
{
private:
    uint64_t _conn_id; // 连接的唯一id并且用于定时器的唯一id
    int _sockfd;       // 连接关联的文件描述符
    bool _active_sign; // 是否启动非活跃销毁，默认false
    EventLoop *_loop;  // 连接所关联的loop也就是连接关联在一个线程上
    ConnStatu _statu;  // 连接的状态
    Socket _socket;    // 套接字操作管理对象
    Channel _channel;  // 连接的事件管理对象
    Buffer _in_buff;   // 输入(读取)缓冲区
    Buffer _out_buff;  // 输出(发送)缓冲区

    /* 连接的回调函数类型 用户使用设置 */
    using ConnectedCallBack = std::function<void(const ConnectionPtr &)>;
    using MessageCallBack = std::function<void(const ConnectionPtr &, Buffer *)>;
    using CloseCallBack = std::function<void(const ConnectionPtr &)>;
    using AnyEventCallBack = std::function<void(const ConnectionPtr &)>;
    ConnectedCallBack _conn_callback;
    MessageCallBack _mess_callback;
    CloseCallBack _close_callback;
    AnyEventCallBack _event_callback;
    /* 由于整个组件的连接都是被管理起来的 因此一旦某个连接需要关闭
       就需要从管理的地方移除掉 */
    CloseCallBack _server_close_callback;

    Any::any _context; // 请求接收处理的上下文

private:
    /* Channel的回调函数 */
    void HandleRead() // 读事件触发
    {
        // 读取socket数据放入缓冲区
        char buff[65536];
        ssize_t ret = _socket.NonBlockRecv(buff, 65535);
        // 出错了不需要直接关闭连接 需要看看还有没有数据待处理
        if (ret < 0)
            return ShutDownInLoop();
        else if (ret == 0)
            return;
        // 写入缓冲区 接口自带写偏移向后移动
        _in_buff.Write_Data(buff, ret);

        // 调用_mess_callback进行业务处理
        // 缓冲区里有数据才调用
        // shared_from_this从当前对象获取share_ptr管理对象
        if (_in_buff.Get_Read_AbleSize() > 0)
            return _mess_callback(shared_from_this(), &_in_buff);
    }
    void HandlWrite() // 写事件触发
    {
        // 将发送缓冲区里的数据进行发送
        ssize_t ret = _socket.NonBlockSend(_out_buff.Get_Read_Start_Pos(), _out_buff.Get_Read_AbleSize());
        // 出错了先看看还有没有数据需要业务处理
        // 如果没有就可以直接关闭连接
        if (ret < 0)
        {
            if (_in_buff.Get_Read_AbleSize() > 0)
                return _mess_callback(shared_from_this(), &_in_buff);
            return CloseInLoop();
        }

        // 发送缓冲区没有数据了那就关闭写事件监控
        if (_out_buff.Get_Read_AbleSize() == 0)
        {
            _channel.DisableWrite();
            // 如果连接是带关闭状态则发送完数据释放连接
            if (_statu == DISCONNECTING)
                return CloseInLoop();
        }
    }
    void HandleClose() // 连接断开触发
    {
        // 连接挂断了 套接字就操作不了
        // 有数据处理就处理完之后就可以关闭连接
        if (_in_buff.Get_Read_AbleSize() > 0)
            return _mess_callback(shared_from_this(), &_in_buff);
        return CloseInLoop();
    }
    void HandleError() // 出错触发
    {
        return HandleClose();
    }
    void HandleEvent() // 任意事件触发
    {
        // 刷新连接活跃度
        if (_active_sign)
            _loop->TimerRefersh(_conn_id);

        // 调用用户的任意事件回调
        if (_event_callback)
            _event_callback();
    }

    // 连接获取后所处的状态下进行各种设置 进行Channel回调设置 启动读事件 设置ConnectedCallBack
    void SetUpInLoop();
    // 实际的释放接口在EventLoop线程中实现
    void CloseInLoop();
    // 并不是实际的发送接口 只是启动写事件和将数据放入缓冲区
    void SendInLoop(char *data, size_t len);
    // 并非实际的连接释放操作，需要判断还有没有数据待处理
    void ShutDownInLoop();
    // 实际连接的启动非活跃销毁接口在EventLoop线程中实现
    void EnableActiveInLoop();
    // 实际连接的取消非活跃销毁接口在EventLoop线程中实现
    void CancelActiveInLoop();
    // 实际连接的切换协议接口在EventLoop线程中实现
    void UpdateGradeInLoop(const Context &context, const ConnectedCallBack &conn_call, const MessageCallBack &mess_call,
                           const CloseCallBack &close_call, const AnyEventCallBack &event_call, );

public:
    Connection(EventLoop *loop, uint64_t conn_id, int socketfd);
    ~Connection();

    int GetFd();        // 获取操作的套接字
    int GetConnId();    // 获取连接的id
    bool IsConnected(); // 判断连接的状态是否处于连接完成状态
    void SetContext(const Any &context); // 设置上下文 连接建立完成时调用
    Any *GetContext();                   // 获取上下文

    /* 设置回调函数 */
    void SetConnectedCall(const ConnectedCallBack &cb);
    void SetMessageCall(const MessageCallBack &cb);
    void SetCloseCall(const CloseCallBack &cb);
    void SetAnyEventCall(const AnyEventCallBack &cb);

    // 供外使用的发送数据接口 实际将数据放到缓冲区并启动写事件监控
    void Send(char *data, size_t len);
    // 供外使用的连接获取后所处的状态下进行各种设置 进行Channel回调设置 启动读事件 设置ConnectedCallBack
    void SetUp();
    // 供外使用的关闭连接接口 并非真正的关闭
    void ShutDown();
    // 启动非活跃销毁
    void EnableActive();
    // 取消非活跃销毁
    void CancelActive();
    // 协议切换 也就是重置上下文以及阶段性处理函数
    void UpdateGrade(const Context &context, const ConnectedCallBack &conn_call, const MessageCallBack &mess_call,
                     const CloseCallBack &close_call, const AnyEventCallBack &event_call, );
};

#endif