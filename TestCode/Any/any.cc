#include <iostream>
#include <typeinfo>
#include <cassert>
#include <string>

class Any{
private:
    class holder{
    public:
        virtual ~holder(){}
        const virtual std::type_info& type() = 0;
        virtual holder *clone() = 0;
    };

    template<class T>
    class placeholder : public holder{
    public:
        placeholder(const T& val) : _val(val) {}

        // 获取子类对象保存的数据类型
        const virtual std::type_info& type() {return typeid(T);}

        // 针对当前的对象自身克隆出一个新的子类对象
        virtual holder *clone() {return new placeholder(_val);}

    public:
        T _val;
    };

    holder* _content;

public:
    Any() : _content(nullptr){}

    template<class T>
    Any(const T& val) : _content(new placeholder<T>(val)) {}

    Any(const Any& other) : _content(other._content ? other._content->clone() : nullptr) {}

    ~Any() {delete _content;}

public:
    Any& swap(Any &other){
        std::swap(_content, other._content);
        return *this;
    }

    // 返回子类对象保存的数据的指针
    template<class T>
    T* get(){
        assert(typeid(T) == _content->type());
        return &((placeholder<T>*)_content)->_val;
    }

    // 重载赋值运算符
    template<class T>
    Any& operator=(const T& val){
        // 为val构造临时的通用容器，与当前容器进行指针交换
        // 临时对象释放的时候，原先的数据也被释放
        Any(val).swap(*this);
        return *this;
    }
    Any& operator=(const Any& other){
        Any(other).swap(*this);
        return *this;
    }
};

class Test{
public:
    Test() {std::cout << "构造" << std::endl;}
    Test(const Test &t) {std::cout << "拷贝" << std::endl;}
    ~Test() {std::cout << "析构" << std::endl;}
};

int main(){
    Any a;
    a = 10;
    int *pa = a.get<int>();
    std::cout << *pa << std::endl;

    a = std::string("hello world");
    std::string *ps = a.get<std::string>();
    std::cout << *ps << std::endl;

    {
        Test t;
        a = t;
    }

    return 0;
}