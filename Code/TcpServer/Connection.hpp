#ifndef __M_CONNECTION_H__
#define __M_CONNECTION_H__

#include "EventLoop.hpp"
#include "Socket.hpp"
#include "Buffer.hpp"
#include <any>

class Connection;
/* 连接的状态 */
typedef enum
{
    DISCONECTED,  // 连接关闭状态
    CONNECTING,   // 连接建立成功待处理状态
    CONNECTED,    // 连接建立完成可进行操作状态
    DISCONNECTING // 连接半关闭状态
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
    std::any _context; // 请求接收处理的上下文

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
    void HandleWrite() // 写事件触发
    {
        // 将发送缓冲区里的数据进行发送
        ssize_t ret = _socket.NonBlockSend(_out_buff.Get_Read_Start_Pos(), _out_buff.Get_Read_AbleSize());
        _out_buff.Move_Read_Offset(ret);
        // 出错了先看看还有没有数据需要业务处理
        // 如果没有就可以直接关闭连接
        if (ret < 0)
        {
            if (_in_buff.Get_Read_AbleSize() > 0)
                return _mess_callback(shared_from_this(), &_in_buff);
            return Release();
        }

        // 发送缓冲区没有数据了那就关闭写事件监控
        if (_out_buff.Get_Read_AbleSize() == 0)
        {
            _channel.DisableWrite();
            // 如果连接是带关闭状态则发送完数据释放连接
            if (_statu == DISCONNECTING)
                return Release();
        }
    }
    void HandleClose() // 连接断开触发
    {
        // 连接挂断了 套接字就操作不了
        // 有数据处理就处理完之后就可以关闭连接
        if (_in_buff.Get_Read_AbleSize() > 0)
            return _mess_callback(shared_from_this(), &_in_buff);
        return Release();
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
            _event_callback(shared_from_this());
    }

    // 连接获取后所处的状态下进行各种设置 进行Channel回调设置 启动读事件 设置ConnectedCallBack
    void SetUpInLoop()
    {
        // 修改连接状态
        assert(_statu == CONNECTING); // 当前状态必须要为刚连接成功状态
        _statu = CONNECTED;

        // 启动读事件监控
        /* 由于读事件监控可能一启动会立即触发事件
           则如果启动了非活跃销毁但是还未来得及添加定时任务
           因此连接获取后才可以启动读事件 不能在构造函数启动
        */
        _channel.EnableRead();

        // 调用连接成功回调函数
        if (_conn_callback)
            _conn_callback(shared_from_this());
    }

    // 实际的释放接口在EventLoop线程中实现
    void CloseInLoop()
    {
        // 修改连接状态
        _statu = DISCONECTED;
        // 移除事件监控
        _channel.Remove();
        // 关闭描述符
        _socket.Close();
        // 取消对应的定时销毁任务(如若存在)
        if (_loop->HasTimer(_conn_id))
            CancelActiveInLoop();
        // 调用关闭回调函数
        if (_close_callback)
            _close_callback(shared_from_this());
        // 移除服务器内部管理的连接信息
        if (_server_close_callback)
            _server_close_callback(shared_from_this());
    }

    // 并不是实际的发送接口 只是启动写事件和将数据放入缓冲区
    void SendInLoop(Buffer buff)
    {
        if (_statu == DISCONECTED)
            return;
        // 写入发送缓冲区
        _out_buff.Write_Data(buff.Get_Read_Start_Pos(), buff.Get_Read_AbleSize());
        // 启动写事件监控
        if (!_channel.WritrAble())
            _channel.EnableWrite();
    }

    // 并非实际的连接释放操作，需要判断还有没有数据待处理
    void ShutDownInLoop()
    {
        // 设置为半关闭状态
        _statu = DISCONNECTING;
        // 判断还有没有数据可读 写入数据时出错会自动关闭
        if (_in_buff.Get_Read_AbleSize() > 0)
            if (_mess_callback)
                _mess_callback(shared_from_this(), &_in_buff);
        // 判断还有没有数据可写
        if (_out_buff.Get_Read_AbleSize() > 0)
            if (!_channel.WritrAble())
                _channel.EnableWrite();
        // 没有数据发送直接关闭
        if (_out_buff.Get_Read_AbleSize() == 0)
            Release();
    }

    // 实际连接的启动非活跃销毁接口在EventLoop线程中实现
    void EnableActiveInLoop(int sec)
    {
        // 将判断标志修改为true
        _active_sign = true;
        // 添加定时销毁任务(如已存在则刷新延迟)
        if (_loop->HasTimer(_conn_id))
            _loop->TimerRefersh(_conn_id); // 刷新
        else
            _loop->TimerAdd(_conn_id, sec, std::bind(&Connection::Release, this));
    }

    // 实际连接的取消非活跃销毁接口在EventLoop线程中实现
    void CancelActiveInLoop()
    {
        // 将判断标志修改为false
        _active_sign = false;
        // 存在定时任务则取消
        if (_loop->HasTimer(_conn_id))
            _loop->TimerCancel(_conn_id);
    }

    // 实际连接的切换协议接口在EventLoop线程中实现
    void UpdateGradeInLoop(const std::any &context, const ConnectedCallBack &conn_call, const MessageCallBack &mess_call,
                           const CloseCallBack &close_call, const AnyEventCallBack &event_call)
    {
        _context = context;
        _conn_callback = conn_call;
        _mess_callback = mess_call;
        _close_callback = close_call;
        _event_callback = event_call;
    }

public:
    Connection(EventLoop *loop, uint64_t conn_id, int socketfd)
        : _conn_id(conn_id), _sockfd(socketfd), _loop(loop), _statu(CONNECTING), _socket(_sockfd), _channel(loop, _sockfd)
    {
        // 设置channel的回调函数
        _channel.SetCloseCallBack(std::bind(&Connection::HandleClose, this));
        _channel.SetEventCallBack(std::bind(&Connection::HandleEvent, this));
        _channel.SetReadCallBack(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallBack(std::bind(&Connection::HandleWrite, this));
        _channel.SetErrorCallBack(std::bind(&Connection::HandleError, this));
    }
    ~Connection() { DBG_LOG("Connection：%p 释放", this); }

    int GetFd() { return _sockfd; }                                  // 获取操作的套接字
    int GetConnId() { return _conn_id; }                             // 获取连接的id
    bool IsConnected() { return _statu == CONNECTED; }               // 判断连接的状态是否处于连接完成状态
    void SetContext(const std::any &context) { _context = context; } // 设置上下文 连接建立完成时调用
    std::any *GetContext() { return &_context; }                     // 获取上下文

    /* 设置回调函数 */
    void SetConnectedCall(const ConnectedCallBack &cb) { _conn_callback = cb; }
    void SetMessageCall(const MessageCallBack &cb) { _mess_callback = cb; }
    void SetCloseCall(const CloseCallBack &cb) { _close_callback = cb; }
    void SetServerCloseCall(const CloseCallBack &cb) { _server_close_callback = cb; }
    void SetAnyEventCall(const AnyEventCallBack &cb) { _event_callback = cb; }

    // 供外使用的连接就绪后所处的状态下进行各种设置 进行Channel回调设置 启动读事件 设置ConnectedCallBack
    void SetUp() { _loop->RunInLoop(std::bind(&Connection::SetUpInLoop, this)); }
    // 供外使用的发送数据接口 实际将数据放到缓冲区并启动写事件监控
    void Send(const char *data, size_t len)
    {
        // 因为传入的data可能是临时空间 因为发送操作只是被压入任务池
        // 可能没有立即被执行 为了防止data的空间被释放后再执行
        // 可以构造一个新的Buffer对象用于存放data的数据
        Buffer buff;
        buff.Write_Data(data, len);
        _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, buff));
    }
    void Release() { _loop->InsertTaskLoop(std::bind(&Connection::CloseInLoop, this)); }
    // 供外使用的关闭连接接口 并非真正的关闭
    void ShutDown() { _loop->RunInLoop(std::bind(&Connection::ShutDownInLoop, this)); }
    // 启动非活跃销毁
    void EnableActive(int sec) { _loop->RunInLoop(std::bind(&Connection::EnableActiveInLoop, this, sec)); }
    // 取消非活跃销毁
    void CancelActive() { _loop->RunInLoop(std::bind(&Connection::CancelActiveInLoop, this)); }
    // 协议切换 也就是重置上下文以及阶段性处理函数
    void UpdateGrade(const std::any &context, const ConnectedCallBack &conn_call, const MessageCallBack &mess_call,
                     const CloseCallBack &close_call, const AnyEventCallBack &event_call)
    {
        // 必须要保证在EventLoop线程中执行
        // 避免新的事件触发后协议还没切换导致数据使用原协议处理
        // 因此这个任务必须先执行
        assert(_loop->IsInLoop() == true);
        _loop->RunInLoop(std::bind(&Connection::UpdateGradeInLoop, this, context, conn_call, mess_call, close_call, event_call));
    }
};

#endif