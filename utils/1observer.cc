#include <algorithm>
#include <vector>
#include <stdio.h>

class Observable;

class Observer {
public:
    virtual ~Observer();
    virtual void update() = 0;
    void observe(Observable* s);

protected:
    Observable* subject_;
};

class Observable {
public:
    void register_(Observer* x);
    void unregister(Observer* x);

    void notifyObservers() {
        for (size_t i = 0; i < observers_.size(); ++i) {
            Observer* x = observers_[i];
            if (x) {
                x->update(); // (3)
            }
        }
    }

private:
    std::vector<Observer*> observers_;
};

Observer::~Observer() {
    subject_->unregister(this); // However dont know subject is alive
}

void Observer::observe(Observable* s) {
    s->register_(this);
    subject_ = s;
}

void Observable::register_(Observer* x) {
    observers_.push_back(x);
}

void Observable::unregister(Observer* x) {
    std::vector<Observer*>::iterator it = std::find(observers_.begin(), observers_.end(), x);
    if (it != observers_.end()) {
        std::swap(*it, observers_.back());
        observers_.pop_back();
    }
}

class Foo : public Observer {
    virtual void update() {
        printf("Foo::update() %p\n", this);
    }
    /*
    Foo(Observable* ob) {
        ob->register_(this); // 1 非线程安全 因为此时foo的构造正在进行中还没有完全结束 this指针不能暴露给其它人
    }
     */
    /*
    Foo();
    void observer(Observable* ob) { // 2 构造正确做法 初始化跟暴露this 分开写
        ob->register_(this);
    }
     */
};

int main() {
    Foo* p = new Foo;
    Observable subject;
    p->observe(&subject);
    subject.notifyObservers();
    delete p;
    subject.notifyObservers();
}
