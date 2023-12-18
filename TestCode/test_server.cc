#include "../Code/Server.hpp"

#include <unordered_map>

std::unordered_map<uint64_t, ConnectionPtr> _conns;
uint64_t connid = 0;
void ConnectionDes(const ConnectionPtr &conn)
{
    _conns.erase(conn->GetConnId());
}

void ConnectedCallBack(const ConnectionPtr &conn)
{
    DBG_LOG("创建Connection: %p", conn.get());
}

void MessageCallBack(const ConnectionPtr &conn, Buffer* buff)
{
    DBG_LOG("%s", buff->Get_Read_Start_Pos());
    buff->Move_Read_Offset(buff->Get_Read_AbleSize());
    std::string str = "收到 Over";
    conn->Send(str.c_str(), str.size());
    conn->ShutDown();
}

void Acceptor(EventLoop* loop, Channel* channel)
{
    int fd = channel->GetFd();
    int newfd = accept(fd, nullptr, nullptr);
    if(newfd < 0) return;

    ++connid;
    ConnectionPtr conn(new Connection(loop, connid, newfd));
    conn->SetMessageCall(std::bind(MessageCallBack, std::placeholders::_1, std::placeholders::_2));
    conn->SetConnectedCall(std::bind(ConnectedCallBack, std::placeholders::_1));
    conn->SetServerCloseCall(std::bind(ConnectionDes, std::placeholders::_1));
    conn->EnableActive(10);
    conn->SetUp();
    _conns.insert(std::make_pair(connid, conn));
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