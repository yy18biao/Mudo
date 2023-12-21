#include "../Code/TcpServer/Server.hpp"

void OnConnected(const ConnectionPtr &conn) {
    DBG_LOG("NEW CONNECTION:%p", conn.get());
}
void OnClosed(const ConnectionPtr &conn) {
    DBG_LOG("CLOSE CONNECTION:%p", conn.get());
}
void OnMessage(const ConnectionPtr &conn, Buffer *buf) {
    DBG_LOG("%s", buf->Get_Read_Start_Pos());
    buf->Move_Read_Offset(buf->Get_Read_AbleSize());
    std::string str = "Hello World";
    conn->Send(str.c_str(), str.size());
}
int main()
{
    Server server(8500);
    server.SetThreadPoolCount(2);
    server.EnableActive(10);
    server.SetCloseCall(OnClosed);
    server.SetConnectedCall(OnConnected);
    server.SetMessageCall(OnMessage);
    server.Start();
    return 0;
}