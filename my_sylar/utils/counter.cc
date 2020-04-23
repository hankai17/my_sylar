#include "my_sylar/thread.hh"
#include "my_sylar/nocopy.hh"

using sylar::Nocopyable;
using sylar::RWMutex;
typedef RWMutex RWMutexType;

// A thread-safe counter
class Counter : Nocopyable {
    // copy-ctor and assignment should be private by default for a class.
public:
    Counter() : value_(0) {}
    Counter& operator=(const Counter& rhs);
    int64_t getValue() const;
    int64_t getAndIncrease();
    friend void swap(Counter& a, Counter& b);

private:
    mutable RWMutexType mutex_; // static
    int64_t value_;
};

int64_t Counter::getValue() const {
    RWMutexType::ReadLock lock(mutex_); // mutable used for there https://blog.csdn.net/Cyang_liu/article/details/65449457
    return value_;
}

int64_t Counter::getAndIncrease() {
    RWMutexType::ReadLock lock(mutex_);
    int64_t ret = value_++;
    return ret;
}

void swap(Counter& a, Counter& b) {
    RWMutexType::ReadLock aLock(a.mutex_);  // potential dead lock
    RWMutexType::ReadLock bLock(b.mutex_);
    int64_t value = a.value_;
    a.value_ = b.value_;
    b.value_ = value;
}

Counter& Counter::operator=(const Counter& rhs)
{
    if (this == &rhs)
        return *this;

    RWMutexType::ReadLock myLock(mutex_);  // potential dead lock
    RWMutexType::ReadLock itsLock(rhs.mutex_);
    value_ = rhs.value_;
    return *this;
}

int main() {
    Counter c;
    c.getAndIncrease();
}

