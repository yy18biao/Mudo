#ifndef __M_SERVER_H__
#define __M_SERVER_H__
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <functional>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "Log.hpp"

#define BUFFER_DEFAULT_SIZE 1024
class Buffer
{
private:
    std::vector<char> _buffer; // 缓冲区空间
    uint64_t _read_idx;        // 读偏移
    uint64_t _write_idx;       // 写偏移

public:
    Buffer() : _buffer(BUFFER_DEFAULT_SIZE), _read_idx(0), _write_idx(0) {}

public:
    // 获取读写偏移量
    uint64_t Get_Read_idx() { return _read_idx; }
    uint64_t Get_Write_idx() { return _write_idx; }

    // 获取当前写入起始地址
    char *Get_Write_Start_Pos()
    {
        // Buffer空间的起始地址加上写偏移量
        return &*_buffer.begin() + _write_idx;
    }

    // 获取当前读取起始地址
    char *Get_Read_Start_Pos()
    {
        // Buffer空间的起始地址加上读偏移量
        return &*_buffer.begin() + _read_idx;
    }

    // 获取前沿空闲空间大小
    uint64_t Get_Front_IdleSize()
    {
        return _read_idx;
    }

    // 获取后沿空闲空间大小
    uint64_t Get_After_IdleSize()
    {
        // 总体空间大小减去写偏移
        return _buffer.size() - _write_idx;
    }

    // 获取可读空间大小
    uint64_t Get_Read_AbleSize()
    {
        // 写偏移 - 读偏移
        return _write_idx - _read_idx;
    }

    // 将读偏移向后移动
    void Move_Read_Offset(uint64_t len)
    {
        // 向后移动大小必须小于可读数据大小
        assert(len <= Get_Read_AbleSize());
        _read_idx += len;
    }

    // 将写偏移向后移动
    void Move_Write_Offset(uint64_t len)
    {
        // 向后移动的大小必须小于当前后沿所剩空间大小
        assert(len <= Get_After_IdleSize());
        _write_idx += len;
    }

    // 确保可写空间是否足够(整体空间足够则移动数据，否则扩容)
    void Ensure_Write_Space(uint64_t len)
    {
        // 如果后沿空间大小足够直接返回即可
        if (Get_After_IdleSize() >= len)
            return;

        // 如果前沿加后沿空间足够，则将整个数据挪到起始位置
        if (len <= Get_After_IdleSize() + Get_Front_IdleSize())
        {
            uint64_t rsz = Get_Read_AbleSize(); // 获取当前数据大小
            // 把可读数据拷贝到起始位置
            std::copy(Get_Read_Start_Pos(), Get_Read_Start_Pos() + rsz, &*_buffer.begin());
            // 更新读写偏移量
            _read_idx = 0;
            _write_idx = rsz;
        }
        // 总体空间都不够，需要扩容
        else
        {
            // 不移动数据直接在写偏移之后扩容足够空间
            _buffer.resize(_write_idx + len);
        }
    }

    // 写入数据
    void Write_Data(const void *data, uint64_t len)
    {
        // 保证有足够空间
        Ensure_Write_Space(len);

        // 拷贝数据进去
        const char *d = (const char *)data;
        std::copy(d, d + len, Get_Write_Start_Pos());
        Move_Write_Offset(len);
        DBG_LOG("写入缓冲区数据成功");
    }

    // 写入String类型数据
    void Write_String(const std::string &data)
    {
        Write_Data(data.c_str(), data.size());
    }

    // 写入BUffer类型数据
    void Write_Buffer(Buffer &data)
    {
        Write_Data(data.Get_Read_Start_Pos(), data.Get_Read_AbleSize());
    }

    // 读取数据
    void Read_Data(void *buff, uint64_t len)
    {
        // 获取的数据大小必须小于可读数据大小
        assert(len <= Get_Read_AbleSize());

        std::copy(Get_Read_Start_Pos(), (char *)Get_Read_Start_Pos() + len, (char *)buff);

        Move_Read_Offset(len);
        DBG_LOG("读取缓冲区数据成功");
    }

    // 将读取的数据当作String
    std::string Read_Data_String(uint64_t len)
    {
        assert(len <= Get_Read_AbleSize());
        std::string str;
        str.resize(len);
        Read_Data(&str[0], len);
        return str;
    }

    // 找到换行字符的位置
    char *FindCRLF()
    {
        void *res = memchr(Get_Read_Start_Pos(), '\n', Get_Read_AbleSize());
        return (char *)res;
    }

    // 读取返回一行数据
    std::string GetLine_Data()
    {
        char *pos = FindCRLF();
        if (pos == nullptr)
            return "";
        return Read_Data_String(pos - Get_Read_Start_Pos() + 1);
    }

    // 清空缓冲区
    void Clear_Buff()
    {
        _read_idx = _write_idx = 0;
        DBG_LOG("清空缓冲区成功");
    }
};

#define LISTEN_SIZE 1024
class Socket
{
private:
    int _sockfd;

public:
    Socket() : _sockfd(-1) {}
    Socket(int fd) : _sockfd(fd) {}
    ~Socket() { Close(); }

public:
    int GetFd() { return _sockfd; }

    bool Create() // 创建套接字
    {
        // int socket(int domain, int type, int protocol);
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_sockfd < 0)
        {
            ERR_LOG("创建套接字失败");
            return false;
        }
        return true;
    }

    bool Bind(const std::string &ip, uint16_t port) // 绑定地址信息
    {
        // int bind(int socket, const struct sockaddr *address, socklen_t address_len);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);

        if (bind(_sockfd, (struct sockaddr *)&addr, len) < 0)
        {
            ERR_LOG("绑定网络地址信息失败");
            return false;
        }
        return true;
    }

    bool Listen(int backlog = LISTEN_SIZE) // 监听
    {
        // int listen(int socket, int backlog);
        if (listen(_sockfd, LISTEN_SIZE) < 0)
        {
            ERR_LOG("设置套接字监听失败");
            return false;
        }
        return true;
    }

    bool Connect(const std::string &ip, uint16_t port) // 向服务器发起连接
    {
        // int connect(int socket, const struct sockaddr *address, socklen_t address_len);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);

        if (connect(_sockfd, (struct sockaddr *)&addr, len) < 0)
        {
            ERR_LOG("连接服务器失败");
            return false;
        }
        return true;
    }

    // 获取新连接
    int Accept()
    {
        // int accept(int socket, struct sockaddr *restrict address, socklen_t *restrict address_len)
        int newfd = accept(_sockfd, nullptr, nullptr);
        if (newfd < 0)
        {
            ERR_LOG("获取新连接失败");
            return false;
        }

        return newfd;
    }

    ssize_t Recv(void *buf, size_t len, int flag = 0) // 接收数据
    {
        // ssize_t recv(int socket, void *buf, size_t len, int flag)
        ssize_t ret = recv(_sockfd, buf, len, flag);
        if (ret <= 0)
        {
            // EAGAIN 当前接收缓冲区没有数据，在非阻塞才会有这个错误
            // EINITR 当前socket的阻塞等待被信号打断了
            if (errno == EAGAIN || errno == EINTR)
                return 0;

            // 出错了
            ERR_LOG("接收数据出错");
            return -1;
        }
        return ret;
    }

    ssize_t NonBlockRecv(void *buf, size_t len) // 非阻塞接收数据
    {
        return Recv(buf, len, MSG_DONTWAIT);
    }

    ssize_t Send(const void *buf, size_t len, int flag = 0) // 发送数据
    {
        // ssize_t send(int socket, const void *buffer, size_t length, int flags);
        ssize_t ret = send(_sockfd, buf, len, flag);
        if (ret < 0)
        {
            ERR_LOG("发送数据出错");
            return -1;
        }
        return ret;
    }

    ssize_t NonBlockSend(const void *buf, size_t len) // 非阻塞发送数据
    {
        return Send(buf, len, MSG_DONTWAIT);
    }

    // 关闭套接字
    void Close()
    {
        if (_sockfd != -1)
        {
            close(_sockfd);
            _sockfd = -1;
        }
    }

    // 开启地址重用
    void ReuseAddress()
    {
        // int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
        int opt = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(int)); // 设置套接字
        opt = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&opt, sizeof(int)); // 设置端口
    }

    // 设置非阻塞
    void NonBlock()
    {
        // int fcntl(int fildes, int cmd, ...);
        int flag = fcntl(_sockfd, F_GETFL, 0);      // 先获取原属性
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK); // 在设置新属性并原属性
    }

    bool CreateServer(uint16_t port, bool block_flag = false) // 创建一个服务端连接
    {
        // 创建套接字
        if (!Create())
            return false;
        // 绑定地址
        if (!Bind("0.0.0.0", port))
            return false;
        // 开始监听
        if (!Listen())
            return false;
        // 设置非阻塞
        if (block_flag)
            NonBlock();
        // 启动地址重用
        ReuseAddress();

        return true;
    }

    bool CreateClient(uint16_t port, const std::string ip) // 创建一个客户端连接
    {
        // 创建套接字
        if (!Create())
            return false;
        // 指向连接服务器
        if (!Connect(ip, port))
            return false;

        return true;
    }
};

using EventCallBack = std::function<void()>;
class Channel
{
private:
    int _fd;
    uint32_t _events;          // 当前需要监控的事件
    uint32_t _revents;         // 当前连接触发的事件
    EventCallBack _read_call;  // 可读事件被触发回调函数
    EventCallBack _write_call; // 可写事件被触发回调函数
    EventCallBack _error_call; // 错误事件被触发回调函数
    EventCallBack _close_call; // 连接断开事件被触发回调函数
    EventCallBack _event_call; // 任意事件被触发回调函数

public:
    Channel(int fd) : _fd(fd) {}
    int GetFd() { return _fd; }

public:
    void SetREvents(uint32_t events)
    {
        _revents = events;
    }

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
    bool ReadAble()
    {
        return (_events & EPOLLIN);
    }

    // 判断当前是否可写
    bool WritrAble()
    {
        return (_events & EPOLLOUT);
    }

    // 启动可读事件监控
    void EnableRead()
    {
        _events |= EPOLLIN;
        /* 与EventLoop相关联，为EventLoop添加事件监控提供接口 */
    }

    // 启动可写事件监控
    void EnableWrite()
    {
        _events |= EPOLLOUT;
        /* 与EventLoop相关联，为EventLoop添加事件监控提供接口 */
    }

    // 关闭可读事件监控
    void DisableRead()
    {
        _events &= ~EPOLLIN;
        /* 与EventLoop相关联，为EventLoop修改事件监控提供接口 */
    }

    // 关闭可写事件监控
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        /* 与EventLoop相关联，为EventLoop修改事件监控提供接口 */
    }

    // 关闭所有事件监控
    void DisableAll()
    {
        _events = 0;
    }

    // 移除监控
    void Remove()
    {
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

#endif