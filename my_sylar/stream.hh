#ifndef __STREAM_HH__
#define __STREAM_HH__

#include <memory>
#include "my_sylar/bytearray.hh"
#include "my_sylar/mbuffer.hh"
#include "socket.hh"
#include "my_sylar/buffer.hh"
#include "thread.hh"
#include "timer.hh"
#include "iomanager.hh"
#include "scheduler.hh"
#include "my_sylar/address.hh"
#include "uri.hh"
#include "my_sylar/ns/ares.hh"
#include <boost/any.hpp>
#include <list>
#include <unordered_map>

namespace sylar {
    class Stream;

    class Stream { // 模式: 依赖反转 很明显的有 纯虚函数跟普通函数"混杂"在一起  buffer由上层管理并传入 rwFix是个高级功能依赖于各版本buffer实现的read write函数
    public:
        typedef std::shared_ptr<Stream> ptr;
        virtual ~Stream() {}; // Can not virutal ~Stream();

        virtual int read(void* buffer, size_t length) = 0;
        virtual int write(const char* buffer, size_t length) = 0;
        virtual int readFixSize(void* buffer, size_t length);
        virtual int writeFixSize(const char* buffer, size_t length);
        virtual int writeFixSize(const void* buffer, size_t length);

        virtual int read(ByteArray::ptr ba, size_t length) = 0;
        virtual int write(ByteArray::ptr ba, size_t length) = 0;
        virtual int readFixSize(ByteArray::ptr ba, size_t length);
        virtual int writeFixSize(ByteArray::ptr ba, size_t length);

        virtual int read(Buffer::ptr buf, size_t length) = 0;
        virtual int write(Buffer::ptr buf, size_t length) = 0;
        virtual int readFixSize(Buffer::ptr buf, size_t length);
        virtual int writeFixSize(Buffer::ptr buf, size_t length);
        virtual int read(Buffer* buf, size_t length);
        virtual int write(Buffer* buf, size_t length);
        virtual int writeFixSize(Buffer* buf, size_t length);

        virtual int read(MBuffer::ptr buf, size_t length) = 0;
        virtual int write(MBuffer::ptr buf, size_t length) = 0;
        virtual int readFixSize(MBuffer::ptr buf, size_t length);
        virtual int writeFixSize(MBuffer::ptr buf, size_t length);

        virtual void close() = 0;
    };

    uint64_t TransferStream(Stream::ptr src, Stream::ptr dst, uint64_t toTransfer = ~0ull);
    uint64_t TransferStream(Stream& src, Stream& dst, uint64_t toTransfer = ~0ull);

    class SocketStream : public Stream {
    public:
        typedef std::shared_ptr<SocketStream> ptr;
        SocketStream(Socket::ptr sock, bool owner = true);
        ~SocketStream();

        virtual int read(void* buffer, size_t length) override;
        virtual int read(Buffer::ptr buf, size_t length) override;
        virtual int read(ByteArray::ptr ba, size_t length) override;
        virtual int read(MBuffer::ptr buf, size_t length) override;

        virtual int write(const char* buffer, size_t length) override;
        virtual int write(Buffer::ptr buf, size_t length) override;
        virtual int write(ByteArray::ptr ba, size_t length) override;
        virtual int write(MBuffer::ptr buf, size_t length) override;

        int sendTo(MBuffer::ptr buf, size_t length, Address::ptr to, int flags = 0);
        int recvFrom(MBuffer::ptr buf, size_t length, Address::ptr from, int flags = 0);

        virtual void close() override;
        void shutdown(int how);

        Socket::ptr getSocket() const { return m_socket; }
        bool isConnected() const;

    protected:
        Socket::ptr         m_socket;
        bool                m_owner;
    };

    //class SocketGram TODO

    class FileStream : public Stream {
    public:
        typedef std::shared_ptr<FileStream> ptr;
        FileStream(int fd);
        ~FileStream();

        virtual int read(void* buffer, size_t length) override { return 0; }
        virtual int read(ByteArray::ptr ba, size_t length) override { return 0; }
        virtual int write(const char* buffer, size_t length) override { return 0; };
        virtual int write(ByteArray::ptr ba, size_t length) override { return 0; };
        virtual int read(Buffer::ptr buf, size_t length) override;
        virtual int write(Buffer::ptr buf, size_t length) override;
        virtual void close() override;

        int getFd() const { return m_fd; }
        //bool isOpened() const;

    protected:
        int                 m_fd;
    };

    class AsyncSocketStream : public SocketStream,
public std::enable_shared_from_this<AsyncSocketStream> {
    public:
        typedef std::shared_ptr<AsyncSocketStream> ptr;
        typedef sylar::RWMutex RWMutexType;
        typedef std::function<bool(AsyncSocketStream::ptr)> connect_callback;
        typedef std::function<void(AsyncSocketStream::ptr)> disconnect_callback;

        enum Error {
            OK = 0,
            TIMEOUT     = -1,
            IO_ERROR    = -2,
            NOT_CONNECT = -3
        };

        struct Ctx {
        public:
            typedef std::shared_ptr<Ctx> ptr;
            virtual ~Ctx() {}
            Ctx();
            uint32_t    sn;
            uint32_t    timeout;
            uint32_t    result;
            bool        timed;

            Scheduler*  scheduler;
            Fiber::ptr  fiber;
            Timer::ptr  timer;

            virtual void doRsp();
            virtual bool doSend(AsyncSocketStream::ptr stream) = 0;
        };

        AsyncSocketStream(Socket::ptr sock, bool owenr = true);
        bool start();
        virtual void close() override;

        bool isAutoConnect() const { return m_autoConnect; }
        void setAutoConnect(bool v) { m_autoConnect = v; }
        connect_callback getConnectCb() const { return m_connectCb; }
        void setConnectCb(connect_callback cb) { m_connectCb = cb; }
        disconnect_callback getDisConnectCb() const { return m_disConnectCb; }
        void setDisConnectCb(disconnect_callback cb) { m_disConnectCb = cb; }

        template <typename T>
        void setData(const T& v) { m_data = v; }

        template <typename T>
        T getData() const {
            try {
                return boost::any_cast<T>(m_data);
            } catch (...) {
            }
            return T();
        }

    protected:
        virtual void doRead();
        virtual void doWrite();
        virtual void startRead();
        virtual void startWrite();
        virtual void onTimeOut(Ctx::ptr ctx);
        virtual Ctx::ptr doRecv() = 0;

        Ctx::ptr getCtx(uint32_t sn);
        Ctx::ptr getAndDelCtx(uint32_t sn);

        template <typename T>
        std::shared_ptr<T> getCtxAs(uint32_t sn) {
            auto ctx = getCtx(sn);
            if (ctx) {
                return std::dynamic_pointer_cast<T>(ctx);
            }
            return nullptr;
        }

        template <typename T>
        std::shared_ptr<T> getAndDelCtx(uint32_t sn) {
            auto ctx = getAndDelCtx(sn);
            if (ctx) {
                return std::dynamic_pointer_cast<T>(ctx);
            }
            return nullptr;
        }

        bool addCtx(Ctx::ptr ctx);
        bool enqueue(Ctx::ptr ctx);
        bool innerClose();
        bool waitFiber();

    protected:
        sylar::FiberSemaphore   m_sem;
        sylar::FiberSemaphore   m_waitsem;
        RWMutexType             m_queueMutex;
        RWMutexType             m_mutex;

        uint32_t                m_sn;
        bool                    m_autoConnect;
        connect_callback        m_connectCb;
        disconnect_callback     m_disConnectCb;
        boost::any              m_data;
        sylar::Timer::ptr       m_timer;
        sylar::IOManager*       m_iomanager;

        std::list<Ctx::ptr>     m_queue;
        std::unordered_map<uint32_t, Ctx::ptr>  m_ctxs;
    };

    class AsyncSocketStreamManager {
    public:
        typedef sylar::RWMutex RWMutexType;
        typedef AsyncSocketStream::connect_callback connect_callback;
        typedef AsyncSocketStream::disconnect_callback disconnect_callback;
        AsyncSocketStreamManager();
        virtual ~AsyncSocketStreamManager() {}

        void add(AsyncSocketStream::ptr stream);
        void clear();
        void setConnection(const std::vector<AsyncSocketStream::ptr>& stream);
        AsyncSocketStream::ptr get();

        template <typename T>
        std::shared_ptr<T> getAs() {
            auto ret = get();
            if (ret) {
                return std::dynamic_pointer_cast<T>(ret);
            }
            return nullptr;
        }

        connect_callback getConnectCb() { return m_connectCb; }
        disconnect_callback getDisConnectCb() { return m_disconnectCb; }
        void setConnectCb(connect_callback v);
        void setDisConnectCb(disconnect_callback v);

    private:
        RWMutexType     m_mutex;
        uint32_t        m_size;
        uint32_t        m_idx;
        connect_callback    m_connectCb;
        disconnect_callback m_disconnectCb;
        std::vector<AsyncSocketStream::ptr>  m_datas;
    };

}

#endif
