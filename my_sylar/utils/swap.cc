#include<iostream>
#include<memory>

int test(std::shared_ptr<int>* addr) {
    std::shared_ptr<int> t;
    t.swap(*addr);
    std::cout<<"test: a: " << *t << std::endl;
    return 0;
}

int main1() {
    std::shared_ptr<int> a(new int(9));
    std::shared_ptr<int>* pa = &a;
    std::cout<<"in main before test: a: " << *a << std::endl;
    test(pa);
    if (a == nullptr) {
        std::cout<<"is nullptr" << std::endl;
    } else {
        std::cout<<"in main after test: a: " << *a << std::endl;
    }
    return 0;
}

//////////////////////////////////////

int test1(std::shared_ptr<int> addr) {
    std::cout<<"test1: addr.use_count(): " << addr.use_count() << std::endl;
    std::shared_ptr<int> t;
    t.swap(addr);
    //t = addr;
    std::cout<<"test1: a: " << *t << ",  t.use_count(): "
             << t.use_count() << "  addr.use_count(): " << addr.use_count() <<std::endl;
    return 0;
}

int main() {
    std::shared_ptr<int> a(new int(9));
    test1(a);
    if (a == nullptr) {
        std::cout<<"is nullptr" << std::endl;
    } else {
        std::cout<<"in main after test: a: " << *a << std::endl;
        std::cout << "a.use_count(): " << a.use_count() <<std::endl;
    }
    return 0;

}
