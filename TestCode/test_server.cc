#include "../Code/Server.hpp"

void CloseCall(Channel* channel)
{
    DBG_LOG("close fd : %d", channel->GetFd());
    channel->Remove();
    delete channel;
}

void ReadCall(Channel* channel)
{
    int fd = channel->GetFd();
    char buff[1024] = {0};
    int ret = recv(fd, buff, 1023, 0);
    if(ret <= 0)
        return CloseCall(channel);

    DBG_LOG("%s", buff);
    channel->EnableWrite();
}

void WriteCall(Channel* channel)
{
    int fd = channel->GetFd();
    std::string data = "hello world!!";
    if(send(fd, data.c_str(), strlen(data.c_str()), 0) < 0)
        return CloseCall(channel);
    channel->DisableWrite();
}

void ErrorCall(Channel* channel)
{
    return CloseCall(channel);
}

void EventCall(EventLoop* loop, Channel* channel, uint64_t timerid)
{
    loop->TimerRefersh(timerid);
}

void Acceptor(EventLoop* loop, Channel* channel)
{
    int fd = channel->GetFd();
    int newfd = accept(fd, nullptr, nullptr);
    if(newfd < 0) return;

    uint64_t timerid = rand() % 10000;
    Channel* chann = new Channel(loop, newfd);
    chann->SetReadCallBack(std::bind(ReadCall, chann));
    chann->SetWriteCallBack(std::bind(WriteCall, chann));
    chann->SetCloseCallBack(std::bind(CloseCall, chann));
    chann->SetErrorCallBack(std::bind(ErrorCall, chann));
    chann->SetEventCallBack(std::bind(EventCall, loop, chann, timerid));
    chann->EnableRead();
    // 添加非活跃连接的销毁任务
    loop->TimerAdd(timerid, 10, std::bind(CloseCall, channel));
    channel->EnableRead();
}

int main()
{
    EventLoop loop;
    Socket server;
    bool ret = server.CreateServer(8000);
    Channel channel(&loop, server.GetFd());
    // 为新连接添加监控
    channel.SetReadCallBack(std::bind(Acceptor, &loop, &channel));
    channel.EnableRead();
    while(1)
    {
        loop.Start();
    }
    server.Close();

    return 0;
}