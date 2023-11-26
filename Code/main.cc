#include "Server.hpp"

int main()
{
    Buffer buffer;

    for (int i = 0; i < 200; ++i)
    {
        std::string str = "hello!!" + std::to_string(i) + '\n';
        buffer.Write_String(str);
    }

    int i = 0;
    while (buffer.Get_Read_AbleSize() > 0)
        std::cout << i++ << "---" << buffer.GetLine_Data() << std::endl;

    buffer.Clear_Buff();

    std::cout << buffer.Read_Data_String(buffer.Get_Read_AbleSize()) << std::endl;
    std::cout << buffer.Get_Read_idx() << std::endl;

    return 0;
}