#include <iostream>
#include <memory>
#include <functional>

class Foo {
public:
    void doit() {
        std::cout << "doit" << std::endl;
        return;
    };
    Foo() {};
    Foo(const Foo& f) {
      std::cout << "copy" << std::endl;
    }
};

int main1() {
    //std::function<void()> f = std::bind(&Foo::doit); // error // static void doit() is ok
    //std::function<void()> f = std::bind(Foo::doit); // error // static void doit() is ok

    Foo foo;
    //std::function<void()> f = std::bind(Foo::doit, foo); // error
    std::function<void()> f = std::bind(&Foo::doit, foo); // ok // bind对成员函数进行绑定时，不要忘记成员函数的隐含参数this。所以在bind的时候要传入对应的类对象或者对象指针
                                                          // bind成员函数必须传对象相关参数作为第一个参数，传啥拷贝啥，不想对象被拷贝就传引用或者指针。
    f();
    return 0;
}

int main() {
    std::shared_ptr<Foo> pFoo(new Foo);
    std::cout << "1pFoo.use_count(): " << pFoo.use_count() << std::endl;
    std::function<void()> f = std::bind(&Foo::doit, pFoo); // bind函数延长了对象的生命周期
    std::cout << "2pFoo.use_count(): " << pFoo.use_count() << std::endl;
    return 0;
}

// https://blog.csdn.net/tsing_best/article/details/25055509
