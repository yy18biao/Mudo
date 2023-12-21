#ifndef __M_ECHO_H__
#define __M_ECHO_H__

#include "../TcpServer/Server.hpp"

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

#endif