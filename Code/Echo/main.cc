#include "Echo.hpp"
#include "../Http/ConText.hpp"

int main()
{
    EchoServer server(8500);
    server.Start();

    return 0;
}