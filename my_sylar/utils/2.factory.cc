#include <map>

#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include "../Mutex.h"

#include <assert.h>
#include <stdio.h>

using std::string;

class Stock : boost::noncopyable
{
public:
    Stock(const string& name)
            : name_(name)
    {
        printf(" Stock[%p] %s\n", this, name_.c_str());
    }

    ~Stock()
    {
        printf("~Stock[%p] %s\n", this, name_.c_str());
    }

    const string& key() const { return name_; }

private:
    string name_;
};

namespace version1
{

// questionable code
    class StockFactory : boost::noncopyable
    {
    public:

        boost::shared_ptr<Stock> get(const string& key)
        {
            muduo::MutexLockGuard lock(mutex_);
            boost::shared_ptr<Stock>& pStock = stocks_[key];
            if (!pStock)
            {
                pStock.reset(new Stock(key));
            }
            return pStock;
        }


    private:
        mutable muduo::MutexLock mutex_;
        std::map<string, boost::shared_ptr<Stock> > stocks_;
    };

}

namespace version2
{

    class StockFactory : boost::noncopyable
    {
    public:
        boost::shared_ptr<Stock> get(const string& key) // 虽然说对象最后确实不用了引用计数为0 完美析构看似完美 但是map容器一直增加仍有内存泄漏
        {
            boost::shared_ptr<Stock> pStock;
            muduo::MutexLockGuard lock(mutex_);
            boost::weak_ptr<Stock>& wkStock = stocks_[key];
            pStock = wkStock.lock();
            if (!pStock)
            {
                pStock.reset(new Stock(key));
                wkStock = pStock;
            }
            return pStock;
        }

    private:
        mutable muduo::MutexLock mutex_;
        std::map<string, boost::weak_ptr<Stock> > stocks_;
    };

}

namespace version3
{

    class StockFactory : boost::noncopyable
    {
    public:

        boost::shared_ptr<Stock> get(const string& key)
        {
            boost::shared_ptr<Stock> pStock;
            muduo::MutexLockGuard lock(mutex_);
            boost::weak_ptr<Stock>& wkStock = stocks_[key];
            pStock = wkStock.lock();
            if (!pStock)
            {
                pStock.reset(new Stock(key),
                             boost::bind(&StockFactory::deleteStock, this, _1)); // 析构对象的同时 同时清理map
                // 这里传this指针仍然有问题 如果工厂先于元素析构 会造成条件竞争
                // 虽然我们设计代码的时候 已经确定了工厂不会析构... 但是社区容不得半点沙子
                // 从而引出了enable_shared_from_this的应用场景
                wkStock = pStock;
            }
            return pStock;
        }

    private:

        void deleteStock(Stock* stock)
        {
            printf("deleteStock[%p]\n", stock);
            if (stock)
            {
                muduo::MutexLockGuard lock(mutex_);
                stocks_.erase(stock->key());  // This is wrong, see removeStock below for correct implementation.
            }
            delete stock;  // sorry, I lied
        }
        mutable muduo::MutexLock mutex_;
        std::map<string, boost::weak_ptr<Stock> > stocks_;
    };

}

namespace version4
{

    class StockFactory : public boost::enable_shared_from_this<StockFactory>, // 这就是enable_shared_from_this的应用场景
                         boost::noncopyable
    {
    public:

        boost::shared_ptr<Stock> get(const string& key)
        {
            boost::shared_ptr<Stock> pStock;
            muduo::MutexLockGuard lock(mutex_);
            boost::weak_ptr<Stock>& wkStock = stocks_[key];
            pStock = wkStock.lock();
            if (!pStock)
            {
                pStock.reset(new Stock(key),
                             boost::bind(&StockFactory::deleteStock,
                                         shared_from_this(), // 场景: factory对象必须得活着 因为我得从factory的map中摘掉一个元素
                                     //-----这种场景虽然延长了factory的生命周期 但也不能说不恰当
                                         _1));
                wkStock = pStock;
            }
            return pStock;
        }

    private:

        void deleteStock(Stock* stock)
        {
            printf("deleteStock[%p]\n", stock);
            if (stock)
            {
                muduo::MutexLockGuard lock(mutex_);
                stocks_.erase(stock->key());  // This is wrong, see removeStock below for correct implementation.
            }
            delete stock;  // sorry, I lied
        }
        mutable muduo::MutexLock mutex_;
        std::map<string, boost::weak_ptr<Stock> > stocks_;
    };

}

class StockFactory : public boost::enable_shared_from_this<StockFactory>,
                     boost::noncopyable
{
public:
    boost::shared_ptr<Stock> get(const string& key)
    {
        boost::shared_ptr<Stock> pStock;
        muduo::MutexLockGuard lock(mutex_);
        boost::weak_ptr<Stock>& wkStock = stocks_[key];
        pStock = wkStock.lock();
        if (!pStock)
        {
            pStock.reset(new Stock(key),
                         boost::bind(&StockFactory::weakDeleteCallback, // 场景: 如果对象还活着我得调他的成员函数 否则忽略之
                                 // bind绑shared_ptr是强回调 绑weak_ptr是弱回调
                                     boost::weak_ptr<StockFactory>(shared_from_this()),
                                     _1));
            wkStock = pStock;
        }
        return pStock;
    }

private:
    static void weakDeleteCallback(const boost::weak_ptr<StockFactory>& wkFactory,
                                   Stock* stock)
    {
        printf("weakDeleteStock[%p]\n", stock);
        boost::shared_ptr<StockFactory> factory(wkFactory.lock());
        if (factory)
        {
            factory->removeStock(stock);
        }
        else
        {
            printf("factory died.\n");
        }
        delete stock;  // sorry, I lied
    }

    void removeStock(Stock* stock)
    {
        if (stock)
        {
            muduo::MutexLockGuard lock(mutex_);
            auto it = stocks_.find(stock->key());
            assert(it != stocks_.end());
            if (it->second.expired())
            {
                stocks_.erase(stock->key());
            }
        }
    }

private:
    mutable muduo::MutexLock mutex_;
    std::map<string, boost::weak_ptr<Stock> > stocks_;
};

void testLongLifeFactory()
{
    boost::shared_ptr<StockFactory> factory(new StockFactory);
    {
        boost::shared_ptr<Stock> stock = factory->get("NYSE:IBM");
        boost::shared_ptr<Stock> stock2 = factory->get("NYSE:IBM");
        assert(stock == stock2);
        // stock destructs here
    }
    // factory destructs here
}

void testShortLifeFactory()
{
    boost::shared_ptr<Stock> stock;
    {
        boost::shared_ptr<StockFactory> factory(new StockFactory);
        stock = factory->get("NYSE:IBM");
        boost::shared_ptr<Stock> stock2 = factory->get("NYSE:IBM");
        assert(stock == stock2);
        // factory destructs here
    }
    // stock destructs here
}

int main()
{
    version1::StockFactory sf1;
    version2::StockFactory sf2;
    version3::StockFactory sf3;
    boost::shared_ptr<version3::StockFactory> sf4(new version3::StockFactory);
    boost::shared_ptr<StockFactory> sf5(new StockFactory);

    {
        boost::shared_ptr<Stock> s1 = sf1.get("stock1");
    }

    {
        boost::shared_ptr<Stock> s2 = sf2.get("stock2");
    }

    {
        boost::shared_ptr<Stock> s3 = sf3.get("stock3");
    }

    {
        boost::shared_ptr<Stock> s4 = sf4->get("stock4");
    }

    {
        boost::shared_ptr<Stock> s5 = sf5->get("stock5");
    }

    testLongLifeFactory();
    testShortLifeFactory();
}
