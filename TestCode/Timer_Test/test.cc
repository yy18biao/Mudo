#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/timerfd.h>

int main()
{
    // 创建定时器文件描述符
    /* 参数一可有两个选择
        1. CLOCK_REALTIME : 以系统时间作为计时基准值
        2. CLOCK_MONOTONIC : 以系统启动时间进行递增的基准值,通常使用这个
    */
   // 参数二,0代表阻塞操作
    int fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if(fd < 0) exit(1);


    // 启动定时器
    // 参数一:定时器文件描述符
    // 参数二:默认设置为0,表示使用相对时间
    /* 参数三:为一个结构体,作用为设置超时的时间
        struct timespec {
            time_t tv_sec; // 秒
            long tv_nsec; // 纳秒
        };
        struct itimerspec {
            struct timespec it_interval; // 第⼀次之后的超时间隔时间
            struct timespec it_value; // 第⼀次超时时间 
        };
    */
    // 参数四:与参数三一样为结构体,作用为接收当前定时器原有的超时时间
    struct itimerspec it;
    // 设置第一次超时时间
    it.it_value.tv_sec = 1;
    it.it_value.tv_nsec = 0;
    // 设置第⼀次之后的超时间隔时间
    it.it_interval.tv_sec = 1;
    it.it_interval.tv_nsec = 0;
    // 启动
    timerfd_settime(fd, 0, &it, nullptr);

    while(1)
    {
        // 循环读取定时器文件里的数据
        uint64_t times;
        int ret = read(fd, &times, 8);
        std::cout << "超时了" << times << "次" << std::endl;
    }
    close(fd);
    return 0;
}