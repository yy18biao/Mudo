#ifndef __M_SOCKET_H__
#define __M_SOCKET_H__

#include "Log.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

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
        if (len == 0)
            return 0;
        return Recv(buf, len, MSG_DONTWAIT);
    }

    ssize_t Send(const void *buf, size_t len, int flag = 0) // 发送数据
    {
        // ssize_t send(int socket, const void *buffer, size_t length, int flags);
        ssize_t ret = send(_sockfd, buf, len, flag);
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                return 0;
            ERR_LOG("发送数据出错");
            return -1;
        }
        return ret;
    }

    ssize_t NonBlockSend(const void *buf, size_t len) // 非阻塞发送数据
    {
        if (len == 0)
            return 0;
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

    bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0", bool block_flag = false) // 创建一个服务端连接
    {
        // 创建套接字
        if (!Create())
            return false;
        // 设置非阻塞
        if (block_flag)
            NonBlock();
        // 绑定地址
        if (!Bind(ip, port))
            return false;
        // 开始监听
        if (!Listen())
            return false;
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

#endif