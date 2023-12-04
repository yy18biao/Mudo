#include "../Code/Server.hpp"

int main()
{
    Socket client;
    client.CreateClient(8000, "127.0.0.1");
    std::string str = "hello world";
    client.Send(str.c_str(), str.size());
    char buf[1024] = {0};
    client.Recv(buf, 1023);
    std::cout << buf << std::endl;
    client.Close();

    return 0;
}