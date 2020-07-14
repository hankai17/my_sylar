# 一个基于C++11简单易用的同步网络编程框架

## 项目特点
- 基于C++11 避免使用裸指针 代码清晰简洁
- 底层使用epoll ET 多线程模式处理网络IO
- 使用无栈协程(swapcontext/boost协程等库)把非阻塞IO操作包装成同步模型 不同于以往以回调方式编程 
- 仅支持linux平台
- 了解更多:[sylar](https://github.com/sylar-yin/sylar) [mordor](https://github.com/mozy/mordor)

## 特性
- 网络库
  - tcp/udp客户端 接口简单易用并且是线程安全的 用户不必关心具体的socket api操作
  - 对套接字多种操作的封装
- 线程库
  - 互斥量
  - 信号量
  - 线程组
- 工具库
  - yaml配置文件的读取
  - 内存池
  - 链接池
  - 简单易用的ssl加解密
  - 其他一些有用的工具

## 编译(Linux)
- 我的编译环境
  - Centos6.9 64 bit + gcc7.1(最低gcc4.4.7)
  - [CMake 3.7.2](https://cmake.org/files/v3.7/cmake-3.7.2.tar.gz) (最低cmake2.8)
- 编译安装
  项目依赖
  - [yaml解析](https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-0.6.3.tar.gz) 编译成动态库并安装  我也编译好了做成了rpm包直接安装即可[百度云](https://pan.baidu.com/s/1fNCRIkIFOim7xgU6VXVtJQ) 提取码6kiu
  - Boost: yum install boost-devel.x86_64

  ```
  
  git clone https://github.com/hankai17/my_sylar.git
  mkdir build && cd build
  cmake ..
  make
  ```

## QA 该库性能怎么样 
- 多线程下使用该库跑ss5代理 消耗1千万协程 无崩溃 没发现明显的内存泄漏
- [context切换测试](https://github.com/hankai17/context_benchmark) 大概在50ns左右
- 击鼓传花测试 参考pingpong_bench.cc libevent_bench.c 在花朵少的情况下my_sylar会低于libevent
- pingpong测试 参考pingpong_qps.cc echo_server.cc 大概14000次/s
- hook暂不支持poll 不支持getaddrinfo/gethostbyname

## test
- kcp简单案例 kcp_test.cc 为kcp客户端  kcp_server_test.cc 为kcp服务端
- ss5测试 tunnel_test.cc 包含有p1 p2端

## TODO
- hostdb
- buffer版本3
- ares测试

## 联系方式
- 邮箱：<hankai17@126.com>
