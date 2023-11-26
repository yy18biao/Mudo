#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <unistd.h>

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
class TimerTask
{
    // 定时器任务类
private:
    uint64_t _id;       // 定时器任务对象ID
    uint32_t _timeout;  // 定时任务的超时时间
    bool _canceled;     // 是否要取消任务的标志
    TaskFunc _task;     // 定时器对象要执行的任务
    ReleaseFunc _release; // 用于删除TimerWheel中保存的定时器对象信息

public:
    TimerTask(uint64_t id, uint32_t timeout, const TaskFunc &cb)
        : _id(id), _timeout(timeout), _task(cb), _canceled(false)
    {}

    // 对象被销毁时执行任务
    ~TimerTask()
    {
        if (_canceled == false) _task();
        _release();
    }

public:
    void SetRelease(const ReleaseFunc& cb)
    {
        _release = cb;
    }

    // 取消任务
    void Cancel() { _canceled = true; }

    uint32_t GetTimeOut(){return _timeout;}
};

using TaskPtr = std::shared_ptr<TimerTask>;
using WeakPtrTask = std::weak_ptr<TimerTask>;
class TimerWheel
{
private:
    int _tick; // 当前的秒针
    int _capacity; // 数组最大容量也就是最大延迟时间
    std::vector<std::vector<TaskPtr>> _wheel;
    std::unordered_map<uint64_t, WeakPtrTask> _timers;

public:
    TimerWheel()
        : _capacity(60), _tick(0), _wheel(_capacity)
    {}

public:
    void RemoveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if(it != _timers.end())
            _timers.erase(it);
    }

    // 添加定时任务
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc& cb)
    {
        TaskPtr pt(new TimerTask(id, delay, cb));
        pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
        _timers[id] = WeakPtrTask(pt);
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
    }

    // 延迟定时任务
    void TimerRefresh(uint64_t id)
    {
        // 通过保存的定时器对象的weak_ptr构造shared_ptr出来，添加到轮子
        auto it = _timers.find(id);
        if(it == _timers.end())
            return;
        
        TaskPtr pt = it->second.lock(); // 获取定时器对象的weak_ptr构造shared_ptr对象
        int delay = pt->GetTimeOut();
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
    }

    // 取消定时任务
    void TimerCancel(uint64_t id) 
    {
        auto it = _timers.find(id);
        if (it == _timers.end()) {
            return;
        }
        TaskPtr pt = it->second.lock();
        if (pt) pt->Cancel();
    }

    // 每秒钟执行一次
    void run()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear();
    }
};

class Test
{
public:
    Test(){std::cout << "构造" << std::endl;}
    ~Test(){std::cout << "析构" << std::endl;}
};

void DelTest(Test* t)
{
    delete t;
}

int main()
{
    TimerWheel tw;
    Test *t = new Test();

    tw.TimerAdd(888, 5, std::bind(DelTest, t));

    for(int i = 0; i < 5; ++i)
    {
        tw.TimerRefresh(888);
        tw.run();
        std::cout << "刷新定时任务" << std::endl;
        sleep(1);
    }
    tw.TimerCancel(888);
    while(1)
    {
        sleep(1);
        std::cout << "......." << std::endl;
        tw.run();
    }

    return 0;
}