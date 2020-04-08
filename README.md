# 一个基于C++11简单易用的网络编程框架

## 项目特点
- 基于C++11 避免使用裸指针 代码清晰简洁
- 底层使用epoll ET 多线程模式处理网络IO
- 结合swapcontext函数把非阻塞IO操作包装成同步模型
- 仅支持linux平台
- 了解更多:[sylar](https://github.com/sylar-yin/sylar) [mordor](https://github.com/mozy/mordor)

## 特性
- 网络库
  - tcp/udp客户端 接口简单易用并且是线程安全的? 用户不必关心具体的socket api操作
  - 对套接字多种操作的封装。
- 线程库
  - 互斥量
  - 信号量
  - 线程组
  - 简单易用的线程池，可以异步或同步执行任务，支持functional 和 lambad表达式。
- 工具库
  - std::cout风格的日志库 代码定位 不支持颜色高亮 异步打印
  - yaml配置文件的读取
  - 基于智能指针的循环池，不需要显式手动释放。
  - 环形缓冲，支持主动读取和读取事件两种模式。
  - 链接池
  - 简单易用的ssl加解密
  - 其他一些有用的工具。

## 编译(Linux)
- 我的编译环境
  - Centos6.9 64 bit + gcc7.1(最低gcc4.7)
  - [CMake 3.7.2](https://cmake.org/files/v3.7/cmake-3.7.2.tar.gz)
- 编译安装
  项目依赖[cmake 3.7.2](https://cmake.org/files/v3.7/cmake-3.7.2.tar.gz)
  [yaml解析](https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-0.6.3.tar.gz) 编译成动态库并安装
  Boost: yum install boost-devel.x86_64
  Protobuf: yum install protobuf-devel.x86_64

  ```
  
  git clone https://github.com/hankai17/my_sylar.git
  mkdir build && cd build
  cmake ..
  make
  ```


## QA
 - 该库性能怎么样？
 参考pingpong_bench.cc libevent_bench.c 在击鼓传花测试中 在花朵少的情况下my_sylar会低于libevent
 - shared_ptr看起来用起来很方便 但是方便的前提是你必须清楚这个对象的use_count 必须清楚哪里用了这个对象 哪里还没有释放
   非常之难受  这跟设计有极大的关系

## 一个iom实例多线程 条件竞争分析
   1. 多线程共享timers 超时条件竞争
   2. 事件到来条件竞争 eg: 读事件正在epoll_wait返回处  此时另一线程又捕获到读事件再次到来
   3. 存在读乱序吗
   4. 存在些乱序吗

   多个iom(accepter io)实例多线程 条件竞争分析



## 联系方式
- 邮箱：<hankai17@126.com>
- https://gitee.com/zjlian/server-framework/blob/master/src/scheduler.h
