#include "../Code/Server.hpp"

int main()
{
    Socket server;
    bool ret = server.CreateServer(8000);
    while(1)
    {
        int newfd = server.Accept();
        if(newfd < 0) continue;
        Socket client(newfd);

        char buf[1024] = {0};
        int ret = client.Recv(buf, 1023);
        if(ret < 0)
        {
            client.Close();
            continue;
        }
        if(client.Send(buf, ret) < 0)
        {
            client.Close();
            continue;
        }
        client.Close();
    }
    server.Close();

    return 0;
}