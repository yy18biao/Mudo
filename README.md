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

## Http协议模块

### Util模块

工具模块，主要提供Http协议所用到的一些工具函数，url编解码，文件读写等

### HttpReques模块

Http请求数据模块，同于保存Http请求数据被解析后的各项请求元素信息

### HttpResponse模块

Http响应数据模块，用于业务处理后设置并保存Http响应数据的各项元素信息

### HttpContext模块

Http请求接受的上下文模块，防止在一次接受数据中不是一个完整的Http请求

### HttpServer模块

整合上述模块，实现简单的接口实现Http服务器的搭建

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
    int GetFd();
    uint32_t GetEvent();
    
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

## Poller模块

对任意描述符进行IO事件监控，也就是对Epoll的封装

**对描述符进行监控，当描述符就绪通过hash表找到对应的Channel对象，获取描述符监控的事件对应的回调函数。**

```cpp
class Poller
{
private:
    int _epfd; // epoll操作句柄
    struct epoll_event _evs[]; // epoll_event数组 监控活跃事件
    std::unordered_map<int, Channel*> _ // hash表管理描述符和Channel对象
private:
    // 对epoll直接操作
    void Update(Channel *channel, int op);
    // 判断一个Channel是否已经被管理
    bool IsChannel(Channel *channel);
    
public:
    // 构造函数中创建epoll句柄
    Poller();
    
    // 添加或修改描述符事件监控
    void Update(Channel* channel);
    
    // 移除描述符事件监控
    void Remove(Channel* channel);
};
```

## Timer模块

timerfd：实现内核每隔一段时间，给进程一次超时事件

timerwheel：实现每次执行Runtimetask，都可以执行一波到期的定时任务

> 因此要实现一个完整的秒级定时器，就需要将这两个功能整合到一起
>
> timerfd设置每秒触发一次定时事件，当事件被出发则运行一次timerwhell的Runtimetask，执行所有的过期定时任务
>
> timerfd的事件监控与触发需要融合EventLoop实现

```cpp
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
    {}

    // 对象被销毁时执行任务
    ~TimerTask()
    {
        if (_canceled == false)
            _task();
        _release();
    }

public:
    void SetRelease(const ReleaseFunc &cb);
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
    void RemoveTimer(uint64_t id);
    // 创建Timer文件描述符
    static int CreateTimerFd();
    // 读取定时器
    void ReadTimerFd();
    // 执行定时任务
    void run();
    // 时间到了之后读取定时器并执行定时任务
    void OnTime();
    // 添加定时任务
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb);
    // 延迟定时任务
    void TimerRefreshInLoop(uint64_t id);
    // 取消定时任务
    void TimerCancelInLoop(uint64_t id);

public:
    TimerWheel(EventLoop *loop)
        : _loop(loop), _capacity(60), _tick(0), _wheel(_capacity)
        , _timerfd(CreateTimerFd()), _timer_channel(new Channel(_loop, _timerfd))
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
    bool HasTimer(uint64_t id);
};
```



## EventLoop模块

### eventfd

使用 eventfd 用于**事件通知功能**

```cpp
#include <sys/eventfd.h>
int eventfd(unsigned int initval, int flags);
```

参数 initval：计数初值

参数flags：

> 1. EFD_CLOEXEC - 禁止进程复制
> 2. EFD_NONBLOCK - 启动非阻塞

返回一个文件描述符用于操作

**eventfd通过read/write/close进行操作，但是read/write进行IO时只能是一个8字节数据**

### EventLoop

与线程一一对应（有多少个线程就有多少个EventLoop对象），进行事件监控以及事件处理

> **为了防止一个连接就绪时在多个线程中都触发了事件，导致线程安全问题，因此需要将一个连接的事件监控以及事件处理都要放在同一个线程中进行**
>
> **为了保证所有操作都在同一个线程中，可以给eventloop模块添加一个任务队列，对连接的所有操作都进行一次封装，并不直接执行操作而是将对连接的操作当作任务添加到任务队列中，当所有的就绪事件处理完了，再去将任务队列中的所有任务全部执行。**
>
> **最终就只需要给任务队列加一把锁就能保证线程安全**
>
> **如果操作本就在线程中，就不需要加入任务队列，直接执行即可**

> EventLoop在实例化对象时必须在线程内部，其thread_id在实例化对象时同步设置，因此需要先创建线程再从线程的入口函数中实例化EventLoop对象

```cpp
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
    // 执行任务池中所有任务
    void RunAllTask();
    // 创建eventfd
    static int CreateEventFd();
    // 读取eventfd
    void ReadEventFd();
    // 写入eventfd用于唤醒阻塞线程
    void WeakUpEventFd();

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
    void RunInLoop(const Func &cb);
    // 将任务加入任务池
    void InsertTaskLoop(const Func &cb);
    // 判断当前线程是否是EventLoop对应的线程
    bool IsInLoop();
    // 添加/修改描述符监控事件
    void UpdateEvent(Channel *channel);
    // 移除描述符监控
    void RemoveEvent(Channel *channel);
    // EventLoop启动功能
    void Start();
    // 添加定时器任务
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);
    // 刷新定时器任务
    void TimerRefersh(uint64_t id);
    // 取消定时任务
    void TimerCancel(uint64_t id);
    // 判断定时任务是否存在
    bool HasTimer(uint64_t id);
};

// 由于EventLoop类在Channel类和TimerWheel类之后定义
// 这两接口又使用到EventLoop类的接口
// 因此需要拿到EventLoop类之后去定义
void Channel::Remove() { _eventloop->RemoveEvent(this); }
void Channel::Update() { _eventloop->UpdateEvent(this); }
void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);
void TimerWheel::TimerRefresh(uint64_t id);
void TimerWheel::TimerCancel(uint64_t id);
```

## Connection模块

对通信连接的整体管理的模块，通信连接的所有操作都是通过这个模块实现的

> 1. 套接字的管理，能够进行套接字的操作
> 2. 连接事件的管理，可读，可写， 错误，挂断，任意
> 3. 缓冲区的管理，便于socket数据的接收和发送
> 4. 协议上下文的管理，记录请求数据的处理过程
>
> **为了避免对连接进行操作的时候，连接已经被释放导致内存访问错误，因此需要使用智能指针shared_ptr对Connection对象进行管理。这样可以保证任意一个地方对Connection对象进行操作时就算释放了也不影响Connection的实际释放**

```cpp
/* 连接的状态 */
typedef enum
{
    DISCONECTED,  // 连接关闭状态
    CONNECTING,   // 连接建立成功待处理状态
    CONNECTED,    // 连接建立完成可进行操作状态
    DISCONNECTING // 连接关闭状态
} ConnStatu;

using ConnectionPtr = std::shared_ptr<Connection>; // Connection智能指针
/* 连接的回调函数类型 用户使用设置 */
using ConnectedCallBack = std::function<void(const ConnectionPtr &)>;
using MessageCallBack = std::function<void(const ConnectionPtr &, Buffer *)>;
using CloseCallBack = std::function<void(const ConnectionPtr &)>;
using AnyEventCallBack = std::function<void(const ConnectionPtr &)>;
class Connection
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
    Any _context;      // 请求接收处理的上下文
    ConnectedCallBack _conn_callback;
    MessageCallBack _mess_callback;
    CloseCallBack _close_callback;
    AnyEventCallBack _event_callback;
    /* 由于整个组件的连接都是被管理起来的 因此一旦某个连接需要关闭
       就需要从管理的地方移除掉 */
    CloseCallBack _server_close_callback;

private:
    /* Channel的回调函数 */
    void HandleRead();  // 读事件触发
    void HandlWrite();  // 写事件触发
    void HandleClose(); // 连接断开触发
    void HandleError(); // 出错触发
    void HandleEvent(); // 任意事件触发

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

    int GetFd();                         // 获取操作的套接字
    int GetConnId();                     // 获取连接的id
    bool IsConnected();                  // 判断连接的状态是否处于连接完成状态
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
```

## Acceptor模块

对监听套接字进行管理

> 1. 创建监听套接字
> 2. 启动读事件监控
> 3. 调用获取新连接成功后的回调函数

```cpp
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
```

## LoopThread模块

将EventLoop与thread整合，可以向外返回所实例化的EventLoop对象，实现主从线程模型

> 1. 创建线程
> 2. 在线程中实例化EventLoop对象

```cpp
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
```

## LoopThreadPool模块

针对LoopThread设计的线程池，便于LoopThread的管理及分配

> 1. 线程数量可配置（0个或多个）
>    1. 在服务器中，主从Reactor模型的主线程只负责新连接获取，从属线程负责新连接事件监控处理。因此线程池中从属线程可以数量为0
> 2. 对所有线程进行管理，本质就是管理0个或多个LoopThread对象
> 3. 提供线程分配的功能，当主线程获取新连接后需要将新连接挂到从属线程
>    1. 0个从属线程时直接分配给主线程的EventLoop
>    2. 多个从属线程时采用RR轮转思想进行分配

```cpp
class LoopThreadPool
{
private:
    int _count;                         // 从属线程数量
    int _loop_idx;                      // loops的偏移量
    EventLoop *_loop;                   // 运行在主线程的EventLoop
    std::vector<LoopThread *> _threads; // 保存所有LoopThread对象
    std::vector<EventLoop *> _loops;    // 保存所有的EventLoop对象

public:
    LoopThreadPool(EventLoop *loop) : _count(0), _loop_idx(0), _loop(loop) {}
    // 设置从属线程数量
    void SetCount(int count) { _count = count; }
    // 创建所有的从属线程
    void Create()
    {
        if (_count == 0)
            return;
        _threads.resize(_count);
        _loops.resize(_count);
        for (int i = 0; i < _count; ++i)
        {
            _threads[i] = new LoopThread();
            _loops[i] = _threads[i]->GetLoop();
        }
    }
    // 获取下一个EventLoop
    EventLoop *GetNextLoop()
    {
        // 如果从属线程数量为0 则只需要返回主线程的EventLoop也就是_loop即可
        if (_count == 0)
            return _loop;
        _loop_idx = (_loop_idx + 1) % _count;
        return _loops[_loop_idx];
    }
};
```

## TCPServer模块

对上述所有模块进行整合，可以简便的完成服务器的搭建

> **服务器功能：**
>
> 1. **设置从属线程池数量**
> 2. **启动服务器**
> 3. **设置各种回调函数 连接建立完成回调、消息回调、关闭连接回调、任意事件回调**
> 4. **是否启动非活跃连接超时销毁**
> 5. **添加定时任务**

> **服务器工作流程：**
>
> 1. **在TCPServer中实例化Acceptor对象和EventLoop对象**
> 2. **将Acceptor对象挂到EventLoop对象中进行事件监控（监听）**
> 3. **Acceptor对象就绪后执行回调函数获取新建连接**
> 4. **对新连接创建Connection中进行管理**
> 5. **对连接所对应的Connecion设置功能回调**
> 6. **用户自选是否启动Connection的非活跃连接的超时销毁**
> 7. **将新连接对应的Connection挂到LoopThreadPool中的从属线程所对应的EventLoop进行事件监控**
> 8. **Connection触发事件执行对应回调**

```cpp
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
    void CreateConnForNew(int fd);
    // 从_conns中移除连接信息
    void RemoveConnInLoop(const ConnectionPtr &conn);
    // 添加定时任务在对应EventLoop中执行
    void AddTimedTaskInLoop(const Func &task, int delay);

public:
    Server(int port) 
        : _port(port), _id(0), _timenum(0)
        , _active_sign(false), _acceptor(&_loop, _port)
        , _loop_pool(&_loop)
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
    bool EnableActive(int timenum) { _timenum = timenum; _active_sign = true; }
    // 添加定时任务
    void AddTimedTask(const Func &task, int delay)
    { _loop.RunInLoop(std::bind(&Server::AddTimedTaskInLoop, this, task, delay)); }
    // 启动服务器
    void Start() { _loop_pool.Create(); _loop.Start(); }
```

## EchoServer

显示服务器

```cpp
class EchoServer
{
private:
    Server _server;

private:
    void OnConnected(const ConnectionPtr &conn)
    {
        DBG_LOG("NEW CONNECTION:%p", conn.get());
    }
    void OnClosed(const ConnectionPtr &conn)
    {
        DBG_LOG("CLOSE CONNECTION:%p", conn.get());
    }
    void OnMessage(const ConnectionPtr &conn, Buffer *buf)
    {
        DBG_LOG("%s", buf->Get_Read_Start_Pos());
        conn->Send(buf->Get_Read_Start_Pos(), buf->Get_Read_AbleSize());
        buf->Move_Read_Offset(buf->Get_Read_AbleSize());
    }

public:
    EchoServer(int port) : _server(port)
    {
        _server.SetThreadPoolCount(2);
        _server.SetCloseCall(std::bind(&EchoServer::OnClosed, this, std::placeholders::_1));
        _server.SetConnectedCall(std::bind(&EchoServer::OnConnected, this, std::placeholders::_1));
        _server.SetMessageCall(std::bind(&EchoServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }

    void Start() { _server.Start(); }
};
```

## HttpUtil模块

实现一些零碎的功能性工具接口

> 1. 读取文件内容
> 2. 向文件写入内容
> 3. URL编解码
> 4. Http状态码和描述信息
> 5. 根据文件后缀名获取mime
> 6. 判断文件是目录还是普通文件
> 7. Http资源路径的有效判断

```cpp
class Util
{
public:
    // 读取文件内容
    static bool ReadFile(const std::string &filename, std::string *buf);
    // 向文件写入数据
    static bool WriteFile(const std::string &filename, const std::string &buf);
    // URL编码
    static std::string UrlEncode(const std::string url, bool convert_space_to_plus);
    // URL解码
    static bool UrlDecode(const std::string url, bool convert_plus_to_space);
    // 响应状态码的描述信息获取
    static std::string StatuDesc(int statu);
    // 根据文件后缀名获取mime
    static std::string GetMime(const std::string &filename);
    // 判断文件是否为目录
    static bool IsDir(const std::string &filename);
    // 判断文件是否为普通文件
    static bool IsReg(const std::string &filename);
    // 字符串分割
    static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> *arry);
    // Http请求的资源路径有效性判断
    static bool ValidPath(const std::string &path);
};
```

## HttpRequest模块

存储Http请求信息

> 1. 请求行：请求方法，URL，协议版本
> 2. 请求头部字段：key: val\r\nkey:val\r\n....
> 3. 正文：提交给服务器的数据

> 接口：
>
> 1. 成员变量设置为公有
> 2. 提供查询字符串以及头部字段的单个查询获取插入功能
> 3. 获取正文长度
> 4. 判断长连接/短连接：Connection：close/keep-alive

```cpp
class Request
{
public:
    std::string _method;                                   // 请求方法
    std::string _path;                                     // 资源路径
    std::string _version;                                  // 协议版本
    std::string _body;                                     // 请求正文
    std::unordered_map<std::string, std::string> _headers; // 头部字段
    std::unordered_map<std::string, std::string> _params;  // 查询字符串
    std::smatch _matches;                                  // 资源路径的正则提取数据

public:
    Request() : _version("HTTP/1.1") {}
    // 重置请求
    void ReSet()
    {
        _method.clear();
        _path.clear();
        _version = "HTTP/1.1";
        _body.clear();
        std::smatch match;
        _matches.swap(match);
        _headers.clear();
        _params.clear();
    }
    // 设置头部字段
    void SetHeader(std::string &key, std::string &val);
    // 判断是否有某个头部
    bool HasHeader(const std::string &key);
    // 获取头部字段的值
    std::string GetHeader(const std::string &key);
    // 插入查询字符串
    void SetParam(const std::string &key, const std::string &val);
    // 判断是否有某个指定的查询字符串
    bool HasParam(const std::string &key) const;
    // 获取指定的查询字符串
    std::string GetParam(const std::string &key) const;
    // 获取正文长度
    size_t ContentLength() const;
    // 判断是否是短链接
    bool Close() const;
};
```

