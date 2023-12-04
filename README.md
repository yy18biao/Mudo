[TOC]

# 实现目标

仿muduo库One Thread One Loop式主从Reactor模型实现高并发服务器

通过实现的高并发服务器组件，可以简洁快速的完成⼀个高性能的服务器搭建。并且，通过组件内提供的不同应用层协议支持，也可以快速完成⼀个高性能应用服务器的搭建（此项目以提供HTTP协议支持为主）

# HTTP服务器

HTTP（Hyper Text Transfer Protocol），超文本传输协议是应用层协议，是⼀种简单的请求-响应协议（客户端根据自己的需要向服务器发送请求，服务器针对请求提供服务，完毕后通信结束）。**本质上HTTP服务器其实就是个TCP服务器，只不过在应用层基于HTTP协议格式进行数据的组织和解析来明确客户端的请求并完成业务处理。**

实现HTTP服务器简单理解，只需要以下几步即可：

1. 搭建⼀个TCP服务器，接收客户端请求。
2. 以HTTP协议格式进行解析请求数据，明确客户端⽬的。
3. 明确客户端请求目的后提供对应服务。
4. 将服务结果⼀HTTP协议格式进行组织，发送给客户端

# Reactor模型

Reactor 模式，是指通过⼀个或多个输入同时传递给服务器进行请求处理时的事件驱动处理模式。服务端程序处理传入多路请求，并将它们同步分派给请求对应的处理线程，Reactor 模式也叫Dispatcher 模式。简单理解就是使用 I/O多路复用统⼀监听事件，收到事件后分发给处理进程或线程，是编写高性能网络服务器的必备技术之⼀。

## 单Reactor单线程

在单个线程中进行事件监控并处理

1. 对所有的客户端进行IO事件监控
2. 那个客户端触发了事件，就去处理哪个客户端（接收请求进行业务处理，进行响应）

> 优点：
>
> 1. 单线程操作，操作都是串行化，思想简单。
> 2. 不需要考虑进程或者线程间的通信与安全问题
>
> 缺点：
>
> 1. 所有事件监控以及业务处理都是在一个线程完成，因此容易造成性能瓶颈
>
>  适用场景：
>
> 1. 客户端较少的业务处理

![image-20231011155655230](https://biao22.oss-cn-guangzhou.aliyuncs.com/image-20231011155655230.png)

## 单Reactor多线程

一个线程专门负责事件监控，事件触发后分发给其他线程进行业务处理

> 优点：
>
> 1. 充分利用CPU多核资源
> 2. 处理效率更高
> 3. 降低了代码的耦合度
>
> 缺点：
>
> 1. 在单个Reactor线程中，包含了所有客户端的事件监控，不利于高并发场景
> 2. 当同时很多个客户端到来，会来不及进行新的客户端连接处理

![image-20231011160054971](https://biao22.oss-cn-guangzhou.aliyuncs.com/image-20231011160054971.png)

## 多Reactor多线程

基于单Reactor多线程的缺点考虑，如果IO的时候又有连接到来导致无法处理。

**因此可以让一个Reactor线程仅仅进行新连接的处理，让其他的Reactor线程进行IO事件监控，IO Reactor线程拿到数据后分给业务处理线程池进行业务处理**

**这种多Reactor多线程模式又叫做主从Reactor模型**

1. **主Reactor线程：进行新连接事件监控**
2. **从属Reactor线程：进行IO事件监控**
3. **业务线程池：进行业务处理**

这种模式充分利用好CPU多核资源，并且可以进行合理的分配。需要注意**执行流并不是越多越好，执行流多了就会增加CPU切换调度的成本**。因此有些高并发并不设置业务线程，业务处理在从属线程中就完成了

![image-20231011161019965](https://biao22.oss-cn-guangzhou.aliyuncs.com/image-20231011161019965.png)

# 项目目标定位

在本次项目中采用的是**One Thread One Loop主从Reactor模型**

与上述多Reactor多线程模式不同的在于，不需要业务处理的线程池。而是在从属Reactor线程中实现

因此从属Reactor线程需要完成三件事

1. **IO事件的监控**
2. **IO操作**
3. **业务处理**

而主Reactor线程只需要**完成对新连接的监控**

# 功能模块划分

对于实现该种**带有协议支持的Reactor模型高性能服务器**，可以将整个项目划分为两大模块

1. **server模块：实现Reactor模型的TCP服务器**
2. **协议模块：对当前的Reactor模型服务器提供应用层协议支持**

## server模块

server模块就是对所有的连接以及线程进行管理，让各个线程在合适的时候做合适的事情，最终完成高性能服务器组件

具体的管理又可以分为三大类：

1. **监听连接管理**
2. **通信连接管理**
3. **超时连接管理**

基于以上的管理思想，将这个模块进行细致的划分又可以划分为以下多个子模块

### Buffer模块

**实现通信套接字的用户态缓冲区**

为什么要有这个缓冲区呢

1. **防止接收到的数据不是一条完整的数据 因此对接收数据进行缓冲**
2. **对于客户端响应的数据 应该实在套接字可写的情况下进行发送**

对于该模块主要功能设计为：**向缓冲区添加数据以及从缓冲区取出数据**

### Socket模块

**对socket套接字的操作进行封装**

功能设计：

1. **创建套接字**
2. **绑定地址信息**
3. **开始监听**
4. **向服务器发起连接**
5. **获取新连接**
6. **接收数据**
7. **发送数据**
8. **关闭套接字**
9. **创建一个监听连接**
10. **创建一个客户端连接**

### Channel模块

**对于一个套接字进行监控事件管理，对于套接字的监控事件在用户态更容易维护，触发事件后的操作流程更加的清晰**

设计：

1. **对监控事件的管理**
   1. **套接字是否可读**
   2. **套接字是否可写**
   3. **对套接字监控可读**
   4. **对套接字监控可写**
   5. **移除可读事件监控**
   6. **移除可写事件监控**
   7. **移除所有事件监控**
2. **对监控事件触发后的处理**
   1. **对于不同事件的回调处理函数**
   2. **明确触发了某个时间之后如何处理**

### Connection模块

**对Buffer模块，Socket模块，Channel模块的一个整体封装，实现了对一个通信套接字的整体的管理，每一个进行数据通信的套接字（也就是accept获取到的新连接）都会使用Connection进行管理**

设计：

1. **关闭连接**
2. **发送数据**
3. **回调函数设置**
   1. **连接建立完成的回调**
   2. **连接有新数据接收成功的回调**
   3. **连接关闭时的回调**
   4. **产生任何事件进行的回调**
4. **协议切换**
5. **启动非活跃连接超时释放**
6. **取消非活跃连接超时释放**

![image-20231123213632452](https://biao22.oss-cn-guangzhou.aliyuncs.com/image-20231123213632452.png)

### Acceptor模块

对监听套接字进行管理，**当获取到一个新建连接的描述符，就需要为这个通信连接封装一个Connection对象设置不同的回调**

设计：

1. **回调函数设置**

   1. **新建连接获取成功的回调设置，由服务器指定**

   ![image-20231123213652556](https://biao22.oss-cn-guangzhou.aliyuncs.com/image-20231123213652556.png)

### TimerQueue模块

**实现固定时间定时任务，可以实现出组件内部对于非活跃连接在指定时间后被释放**

设计：

1. 添加定时任务
2. 刷新定时任务，可以使一个定时任务重新开始计时
3. 取消定时任务

### Poller模块

**对epoll进⾏封装，对任意描述符进行IO事件监控**

设计：

1. 添加、修改、移除事件监控
   1. 通过Channel模块实现

### EventLoop模块

**进行事件监控管理的模块，对Poller模块，TimerQueue模块，Socket模块的整体封装。对于服务器中的所有事件都是由这个模块来完成的，⼀个对象对应⼀个线程，每一个Connection连接都会绑定一个EventLoop模块和线程，外界对于连接的所有操作都是要放到同一个线程中进行的。对所有的连接进行事件监控，连接触发事件后调用回调进行处理，对连接的所有操作都要放到EventLoop线程中执行。**

设计：

1. 将连接的操作任务添加到任务队列
2. 定时任务的添加、刷新、取消

![image-20231123213726355](https://biao22.oss-cn-guangzhou.aliyuncs.com/image-20231123213726355.png)

### TcpServer模块

**对前面所有的子模块进行整合，提供给用户用于搭建高性能服务器的模块，让使用者更加轻便的完成一个服务器的搭建。**

设计：

1. 对于监听连接的管理
2. 对于通信连接的管理
3. 对于超时连接的管理
4. 对于事件监控的管理
5. 事件回调函数的设置
   1. 对于连接产生事件该如何处理，也就是一个事件的回调函数是由使用者去设置好传给TcpServer模块，再由TcpServer模块传给各个Connection连接

# 实现模块

## 日志宏编写

```cpp
#include <iostream>
#include <ctime>

#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL DBG
#define LOG(level, format, ...)\
do{\
    if (level < LOG_LEVEL) break;\
    time_t t = time(NULL);\
    struct tm *ltm = localtime(&t);\
    char tmp[32] = {0};\
    strftime(tmp, 31, "%H:%M:%S", ltm);\
    fprintf(stdout, "[%s %s:%d] " format "\n", tmp, __FILE__, __LINE__, ##__VA_ARGS__);\
}while(0)
#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)
```

## Buffer模块

提供功能：存储数据，取出数据

实现思想：

1. 实现缓冲区得有一块内存空间：vector< char >
   1. 考虑默认空间大小
   2. 当前读取数据的起始位置
   3. 当前写入数据的起始位置
2. 写入数据
   1. 当前写入位置指向哪里就从哪里开始
   2. 若写入的空间不够了
      1. **考虑缓冲区整体所剩的空间是否足够，如果足够将缓冲区的数据整体挪动至数组的起始位置留出所剩空间进行写入**
      2. **整体所剩空间不够时，直接扩容整个数组从当前位置继续写入**
3. 读取数据
   1. 当前读取位置指向哪里就从哪里开始读
   2. 可读数据大小：**当前写入位置减去当前读取位置**

```cpp
class Buffer
{
private:
    std::vector<char> _buffer;  // 缓冲区空间
    uint64_t _read_idx;         // 读偏移
    uint64_t _write_idx;        // 写偏移
public:
    char* Get_Write_Start_Pos(); // 获取当前写入起始地址
    char* Get_Read_Start_Pos();  // 获取当前读取起始地址
    uint64_t Get_Front_IdleSize(); // 获取前沿空闲空间大小
    uint64_t Get_After_IdleSize(); // 获取后沿空闲空间大小
    uint64_t Get_Read_AbleSize(); // 获取可读空间大小
    void Move_Read_Offset(uint64_t len); // 将读偏移向后移动  
    void Move_Write_Offset(uint64_t len); // 将写偏移向后移动
    void Ensure_Write_Space(uint64_t len); // 确保可写空间是否足够(整体空间足够则移动数据，否则扩容)
    void Write_Data(const void* data, uint64_t len); // 写入数据
    void Write_String(const std::string& data); // 写入String类型数据
    void Write_Buffer(Buffer& data); // 写入BUffer类型数据
    void Read_Data(void* buff, uint64_t len); // 读取数据
    std::string Read_Data_String(uint64_t len); // 将读取的数据当作String
    char *FindCRLF(); // 找到换行字符的位置
    std::string GetLine_Data(); // 读取返回一行数据
    void Clear_Buff(); // 清空缓冲区
};
```

## Socket模块

细节：

1. 需要开启地址端口重用
2. 套接字需要设置为非阻塞

```cpp
class Socket
{
private:
	int _sockfd;

public:
    Socket();
    Socket(int fd);
    ~Socket();
    
public:
    bool Create(); // 创建套接字
    bool Bind(const std::string &ip, uint16_t port); // 绑定地址信息
    bool Listen(int backlog = LISTEN_SIZE); // 监听
    bool Connect(const std::string &ip, uint16_t port); // 向服务器发起连接
    int Accept(); // 获取新连接
    ssize_t Recv(void* buf, size_t len, int flag = 0); // 接收数据
    ssize_t NonBlockRecv(void *buf, size_t len); // 非阻塞接收数据
    ssize_t Send(void* buf, size_t len, int flag = 0); // 发送数据
    ssize_t NonBlockSend(void *buf, size_t len); // 非阻塞发送数据
    void Close(); // 关闭套接字
    void ReuseAddress(); // 开启地址重用
    void NonBlock(); // 设置非阻塞
    bool CreateServer(uint16_t port); // 创建一个服务端连接
    bool CreateClient(uint16_t port); // 创建一个客户端连接
};
```

## Channel模块

对套接字进行监控事件管理，使用Epoll进行管理

> EPOLLIN          可读
>
> EPOLLOUT      可写
>
> EPOLLRDHUP 连接断开
>
> EPOLLPRI        优先数据
>
> EPOLLERR       出错
>
> EPOLLHUP      刚创建出来什么也没干直接断开

```cpp
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
    Channel();
public:
    void SetREvents(uint32_t events); // 设置当前连接触发的事件
    
    /* 设置各个回调函数 */
    void SetReadCallBack(const EventCallBack &cb);
    void SetWriteCallBack(const EventCallBack &cb);
    void SetErrorCallBack(const EventCallBack &cb);
    void SetCloseCallBack(const EventCallBack &cb);
    void SetEventCallBack(const EventCallBack &cb);

    bool ReadAble();     // 判断当前是否可读
    bool WritrAble();    // 判断当前是否可写
    /* 与EventLoop相关联，为EventLoop添加事件监控提供接口 */
    void EnableRead();   // 启动可读事件监控
    void EnableWrite();  // 启动可写事件监控
    
    /* 与EventLoop相关联，为EventLoop修改事件监控提供接口 */
    void DisableRead();  // 关闭可读事件监控
    void DisableWrite(); // 关闭可写事件监控
    
    void DisableAll();   // 关闭所有事件监控
    void Remove();       // 移除监控
    
    // 一旦连接触发了事件就调用这个函数
    // 决定了触发了什么事件应该调用哪个函数
    void HandleEvent();  
};
```