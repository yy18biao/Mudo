#include "../Code/Server.hpp"

#include <unordered_map>

std::unordered_map<uint64_t, ConnectionPtr> _conns;
uint64_t connid = 0;
LoopThreadPool *loop_pool;
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
    ConnectionPtr conn(new Connection(loop_pool->GetNextLoop(), connid, fd));
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
    loop_pool = new LoopThreadPool(&loop);
    loop_pool->SetCount(2);
    loop_pool->Create();
    Acceptor apt(&loop, 8000);
    // 为新连接添加监控
    apt.SetAcceptCallBack(std::bind(Accept, std::placeholders::_1));
    apt.Listen();
    loop.Start();

    return 0;
}