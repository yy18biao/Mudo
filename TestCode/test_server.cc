#include "../Code/Server.hpp"

#include <unordered_map>

std::vector<LoopThread> threads(2);
std::unordered_map<uint64_t, ConnectionPtr> _conns;
uint64_t connid = 0;
int loopidx = 0;
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

void Accept(int fd)
{
    ++connid;
    loopidx = (loopidx + 1) % 2;
    ConnectionPtr conn(new Connection(threads[loopidx].GetLoop(), connid, fd));
    conn->SetMessageCall(std::bind(MessageCallBack, std::placeholders::_1, std::placeholders::_2));
    conn->SetConnectedCall(std::bind(ConnectedCallBack, std::placeholders::_1));
    conn->SetServerCloseCall(std::bind(ConnectionDes, std::placeholders::_1));
    conn->EnableActive(10);
    conn->SetUp();
    _conns.insert(std::make_pair(connid, conn));
    DBG_LOG("新的连接");
}

int main()
{
    EventLoop loop;
    Acceptor apt(&loop, 8000);
    // 为新连接添加监控
    apt.SetAcceptCallBack(std::bind(Accept, std::placeholders::_1));
    apt.Listen();
    while(1) loop.Start();

    return 0;
}