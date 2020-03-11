# 一个基于C++11简单易用的网络编程框架

## 项目特点
- 基于C++11 避免使用裸指针 代码清晰简洁
- 底层使用epoll ET 多线程模式处理网络IO
- 结合swapcontext函数把非阻塞IO操作包装成同步模型
- 仅支持linux平台
- 了解更多:[sylar](https://github.com/sylar-yin/sylar) [mordor](https://github.com/mozy/mordor)

## 特性
- 网络库
  - tcp/udp客户端，接口简单易用并且是线程安全的，用户不必关心具体的socket api操作。
  - tcp服务器，使用非常简单，只要实现具体的tcp会话（TcpSession类）逻辑,使用模板的方式可以快速的构建高性能的服务器。
  - 对套接字多种操作的封装。
- 线程库
  - 使用线程实现的简单易用的定时器。
  - 信号量。
  - 线程组。
  - 简单易用的线程池，可以异步或同步执行任务，支持functional 和 lambad表达式。
- 工具库
  - 文件操作。
  - std::cout风格的日志库，支持颜色高亮、代码定位、异步打印。
  - INI配置文件的读写。
  - 监听者模式的消息广播器。
  - 基于智能指针的循环池，不需要显式手动释放。
  - 环形缓冲，支持主动读取和读取事件两种模式。
  - mysql链接池，使用占位符（？）方式生成sql语句，支持同步异步操作。
  - 简单易用的ssl加解密黑盒，支持多线程。
  - 其他一些有用的工具。
  - 命令行解析工具，可以很便捷的实现可配置应用程序

## 编译(Linux)
- 我的编译环境
  - Centos6.9 64 bit + gcc7.1(最低gcc4.7)
  - [cmake 3.7.2](https://cmake.org/files/v3.7/cmake-3.7.2.tar.gz)
- 编译安装
  项目依赖[cmake 3.7.2](https://cmake.org/files/v3.7/cmake-3.7.2.tar.gz)
  [yaml解析](https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-0.6.3.tar.gz) 编译成动态库并安装

  ```
  
  git clone https://github.com/hankai17/my_sylar.git
  mkdir build && cd build
  cmake ..
  make
  ```


## QA
 - 该库性能怎么样？


## 联系方式
- 邮箱：<hankai17@126.com>
