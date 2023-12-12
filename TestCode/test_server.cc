#include "../Code/Server.hpp"

void CloseCall(Channel* channel)
{
    std::cout << "close: " << channel->GetFd() << std::endl;
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

    channel->EnableWrite();
    std::cout << buff << std::endl;
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

void EventCall(Channel* channel)
{
    std::cout << "EventCall" << std::endl;
}

void Acceptor(Poller* poller, Channel* channel)
{
    int fd = channel->GetFd();
    int newfd = accept(fd, nullptr, nullptr);
    if(newfd < 0) return;

    Channel* chann = new Channel(poller, newfd);
    chann->SetReadCallBack(std::bind(ReadCall, chann));
    chann->SetWriteCallBack(std::bind(WriteCall, chann));
    chann->SetCloseCallBack(std::bind(CloseCall, chann));
    chann->SetErrorCallBack(std::bind(ErrorCall, chann));
    chann->SetEventCallBack(std::bind(EventCall, chann));
    chann->EnableRead();
}

int main()
{
    Poller poller;
    Socket server;
    bool ret = server.CreateServer(8000);
    Channel channel(&poller, server.GetFd());
    // 为新连接添加监控
    channel.SetReadCallBack(std::bind(Acceptor, &poller, &channel));
    channel.EnableRead();
    while(1)
    {
        std::vector<Channel*> channels;
        poller.Poll(&channels);
        for(auto& chann : channels)
        {
            chann->HandleEvent();
        }
    }
    server.Close();

    return 0;
}