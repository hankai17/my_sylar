#include "stream.hh"
#include "util.hh"
#include "macro.hh"
#include "hook.hh"
#include "log.hh"
#include "endian.hh"
#include <vector>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>

namespace sylar {

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    static void ReadOne(Stream& src, Buffer*& buffer, size_t len, size_t& result) {
        result = src.read(buffer, len);
        SYLAR_LOG_ERROR(g_logger) << "ReadOne result: " << result;
    }

    static void WriteOne(Stream& dst, Buffer*& buffer) {
        SYLAR_LOG_ERROR(g_logger) << "WriteOne " << buffer->readableBytes();
        if (buffer->readableBytes() > 0) {
            dst.writeFixSize(buffer, buffer->readableBytes());
        }
    }

    // 与ss服务器建联 传参是组ss包中的ip或域名及端口
    Stream* tunnel(Uri::ptr proxy, IPAddress::ptr targetIP, 
          const std::string& targetDomain, uint16_t targetPort, uint8_t version, std::string& cli) {
        SYLAR_ASSERT(version == 4 || version == 5);
        SYLAR_ASSERT(version == 5 || !targetIP 
              || targetIP->getFamily() == AF_INET); // 版本4的targetIP必须非空且是ipv4
        SYLAR_ASSERT(targetIP || !targetDomain.empty());
        std::string buffer;
        int size = 0;
        buffer.resize(std::max<size_t>(targetDomain.size() + 1u, 16u) + 9); // 7?

            Address::ptr addr = proxy->createAddress(); 
            Socket::ptr sock = 0 ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
            if (!sock->connect(addr)) {
                SYLAR_LOG_ERROR(g_logger) << "connect to proxy failed"; 
                return nullptr; 
            }
            sock->setRecvTimeout(1000);
        Stream::ptr stream(new SocketStream(sock), [](Stream*){});

        if (false && version == 5) { // 发510 收50
            buffer[0] = version;
            buffer[1] = 1;
            buffer[2] = 0;
            if (stream->writeFixSize(buffer.data(), 3) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "stream send failed"; 
                return nullptr;
            }
            if (stream->readFixSize(&buffer[0], 2) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "stream read failed"; 
                return nullptr; 
            }
            SYLAR_ASSERT(buffer[0] == 5 && 
                  (uint8_t)buffer[1] != 0xFF && buffer[1] == 0); // 只支持无密码
        }
        buffer[0] = version;
        buffer[1] = 1;
        int addrType = 0;

        if (version == 4) {
            // TODO
        } else {
            buffer[2] = 0;    
            if (targetIP) {
                if (targetIP->getFamily() == AF_INET) { // IPv4
                    buffer[3] = 1;
                    addrType = 1;
                    *(unsigned int*)&buffer[4] = sylar::byteswapOnLittleEndian( (unsigned int)(((sockaddr_in*)targetIP->getAddr())->sin_addr.s_addr) );
                    size = 7;
                } else { 
                    // IPv6
                    //buffer[3] = 4;
                    //memcpy(&buffer[4], ) ? // ipv6用主机序?
                }
            } else {
                buffer[3] = 3;
                addrType = 3;
                buffer[4] = (uint8_t)targetDomain.size();
                memcpy(&buffer[5], targetDomain.c_str(), targetDomain.size());
                size = 5 + targetDomain.size();
            }
            if (targetIP) {
                *(uint16_t*)&buffer[size] = sylar::byteswapOnLittleEndian(targetIP->getPort());
            } else {
                *(uint16_t*)&buffer[size] = sylar::byteswapOnLittleEndian(targetPort);
            }
            size += 2;
        }

        // 发510[1|3|4]ip port
        if (stream->writeFixSize(buffer.data(), size) <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "stream send1 failed"; 
            return nullptr;
        }

        // 收50
        if (stream->readFixSize(&buffer[0], 2) <= 0) {
            SYLAR_LOG_ERROR(g_logger) << "stream read1 failed"; 
            return nullptr;
        }
        SYLAR_ASSERT( (version == 4 && buffer[0] == 0) || 
              (version == 5 && buffer[0] == 5) );
        SYLAR_ASSERT( (version == 4 && buffer[1] == 0x5a) || 
              (version == 5 && buffer[1] == 0) ); 
        if (version == 4) {
            // TODO
        } else {
            cli.resize(40);
            cli[0] = 5;
            cli[1] = 0;
            if (stream->readFixSize(&buffer[0], 2) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "stream read2 failed"; 
                return nullptr;
            }
            cli[2] = buffer[0];
            cli[3] = buffer[1];
            SYLAR_ASSERT(buffer[0] == 0);
            if (addrType == 3) {
                size = 3 + 2;
            } else if (addrType == 1) {
                size = 6;
            } else if (addrType == 4) {
                size = 18;
            } else {
                SYLAR_ASSERT(false);
            }
            if (stream->readFixSize(&buffer[0], size) <= 0) {
                SYLAR_LOG_ERROR(g_logger) << "stream read3 failed"; 
                return nullptr; 
            }
            strncpy(&cli[4], &buffer[0], size);
        }
        return stream.get();
    }

    uint64_t TransferStream(Stream& src, Stream& dst, uint64_t toTransfer) {
        Buffer::ptr buf1(new Buffer);
        Buffer::ptr buf2(new Buffer);
        Buffer* readBuffer, *writeBuffer;
        size_t chunkSize = 65535; // Buffer support max 65535
        size_t todo, readResult;
        uint64_t totalRead = 0;
        if (toTransfer == 0) {
            return 0;
        }
        readBuffer = buf1.get();
        todo = chunkSize;
        if (toTransfer - totalRead < todo) {
            todo = toTransfer - totalRead;
        }
        // Pre read
        ReadOne(src, readBuffer, todo, readResult);
        totalRead += readResult;
        if (readResult == 0) {
            return totalRead;
        }

        std::vector<std::function<void()> > deferGroups;
        std::vector<Fiber::ptr> fibers;
        deferGroups.resize(2);
        fibers.resize(2);
        //fibers[0].reset(new Fiber(nullptr));
        //fibers[1].reset(new Fiber(nullptr));
        deferGroups[0] = std::bind(&ReadOne, std::ref(src), std::ref(readBuffer), todo, std::ref(readResult));
        deferGroups[1] = std::bind(&WriteOne, std::ref(dst), std::ref(writeBuffer));

        while (totalRead < toTransfer) {
            writeBuffer = readBuffer;
            if (readBuffer == buf1.get()) {
                readBuffer = buf2.get();
            } else {
                readBuffer = buf1.get();
            }
            todo = chunkSize;
            if (toTransfer - totalRead < todo) {
                todo = toTransfer - totalRead;
            }
            ParallelDo(deferGroups, fibers);
            totalRead += readResult;
            if (readResult == 0) {
                SYLAR_LOG_ERROR(g_logger) << "totalRead: " << totalRead;
                return totalRead;
            }
        }
        writeBuffer = readBuffer;
        WriteOne(dst, writeBuffer);
        SYLAR_LOG_ERROR(g_logger) << "totalRead: " << totalRead;
        return totalRead;
    }

    int Stream::readFixSize(void* buffer, size_t length) {
        size_t offset = 0;
        int64_t left = length;
        while (left > 0) {
            int64_t len = read((char*)buffer + offset, left);
            if (len <= 0) {
                return len;
            }
            offset += len;
            left -= len;
        }
        return length;
    }

    int Stream::readFixSize(ByteArray::ptr ba, size_t length) {
        int64_t left = length;
        while (left > 0) {
            int64_t len = read(ba, left); // ba remember the m_postion
            if (len <= 0) {
                return len;
            }
            left -= len;
        }
        return length;
    }

    int Stream::writeFixSize(const char* buffer, size_t length) {
        size_t offset = 0;
        int64_t left = length;
        while (left > 0) {
            int64_t len = write((char*)buffer + offset, left);
            if (len <= 0) {
                return len;
            }
            offset += len;
            left -= len;
        }
        return length;
    }

    int Stream::writeFixSize(const void* buffer, size_t length) {
        return writeFixSize(static_cast<const char*>(buffer), length);
    }

    int Stream::writeFixSize(ByteArray::ptr ba, size_t length) {
        int64_t left = length;
        while (left > 0) {
            int64_t len = write(ba, left);
            if (len <= 0) {
                return len;
            }
            left -= len;
        }
        return length;
    }

    int Stream::readFixSize(Buffer::ptr buf, size_t length) {
        int64_t left = length;
        while (left > 0) {
            int64_t len = read(buf, left);
            if (len <= 0) {
                return len;
            }
            left -= len;
        }
        return length;
    }

    int Stream::writeFixSize(Buffer::ptr buf, size_t length) {
        int64_t left = length;
        while (left > 0) {
            int64_t len = write(buf, left);
            if (len <= 0) {
                return len;
            }
            left -= len;
        }
        return length;
    }

    int Stream::read(Buffer* buf, size_t length) {
        std::shared_ptr<Buffer> buffer(buf, [](Buffer* ptr){});
        return read(buffer, length);
    }

    int Stream::write(Buffer* buf, size_t length) {
        std::shared_ptr<Buffer> buffer(buf, [](Buffer* ptr){});
        return write(buffer, length);
    }

    int Stream::writeFixSize(Buffer* buf, size_t length) {
        std::shared_ptr<Buffer> buffer(buf, [](Buffer* ptr){});
        return writeFixSize(buffer, length);
    }

    SocketStream::SocketStream(Socket::ptr sock, bool owner)
    : m_socket(sock),
    m_owner(owner) {
    }

    SocketStream::~SocketStream() {
        if (m_owner && m_socket) {
            m_socket->close();
        }
    }

    bool SocketStream::isConnected() const {
        return m_socket && m_socket->isConnected();
    }

    int SocketStream::read(void* buffer, size_t length) {
        if (!isConnected()) {
            return -1;
        }
        return m_socket->recv(buffer, length);
    }

    int SocketStream::read(ByteArray::ptr ba, size_t length) {
        if (!isConnected()) {
            return -1;
        }
        std::vector<iovec> iovs;
        ba->getWriteBuffers(iovs, length);
        int ret = m_socket->recv(&iovs[0], iovs.size());
        if (ret > 0) {
            ba->setPosition(ba->getPosition() + ret);
        }
        return ret;
    }

    int SocketStream::write(const char* buffer, size_t length) {
        if (!isConnected()) {
            return -1;
        }
        return m_socket->send(buffer, length);
    }

    int SocketStream::write(ByteArray::ptr ba, size_t length) {
        if (!isConnected()) {
            return -1;
        }
        std::vector<iovec> iovs;
        ba->getReadBuffers(iovs, length);
        int ret = m_socket->send(&iovs[0], length);
        if (ret > 0) {
            ba->setPosition(ba->getPosition() + ret);
        }
        if (ret < 0) {
            std::cout<<"errno: " << errno
            << " strerrno: " << strerror(errno) << std::endl;
        }
        return ret;
    }

    int SocketStream::read(Buffer::ptr buf, size_t length) {
        if (!isConnected()) {
            return -1;
        }
        int err;
        int ret = buf->orireadFd(m_socket->getSocket(), length, &err);
        return ret;
    }

    int SocketStream::write(Buffer::ptr buf, size_t length) {
        if (!isConnected()) {
            return -1;
        }
        int err;
        int ret = buf->writeFd(m_socket->getSocket(), length, &err);
        return ret;
    }

    void SocketStream::close() {
        if (m_socket) {
            m_socket->close();
        }
    }

    FileStream::FileStream(int fd)
      : m_fd(fd) {
    }

    int FileStream::read(Buffer::ptr buf, size_t length) {
        int err;
        int ret = buf->orireadFd(m_fd, length, &err);
        return ret;
    }

    int FileStream::write(Buffer::ptr buf, size_t length) {
        int err;
        int ret = buf->writeFd(m_fd, length, &err);
        return ret;
    }

    void FileStream::close() {
        if (m_fd > 0) {
            close_f(m_fd);
        }
    }

    FileStream::~FileStream() {
        close();
    }

    AsyncSocketStream::AsyncSocketStream(Socket::ptr sock, bool owenr)
    : SocketStream(sock, owenr),
    m_waitsem(2),
    m_sn(0),
    m_autoConnect(false),
    m_iomanager(nullptr) {
    }

    bool AsyncSocketStream::waitFiber() {
        m_waitsem.wait();
        m_waitsem.wait();
        return true;
    }

    bool AsyncSocketStream::innerClose() {
        if (isConnected() && m_disConnectCb) {
            m_disConnectCb(shared_from_this());
        }
        SocketStream::close();
        m_sem.notify();
        std::unordered_map<uint32_t, Ctx::ptr> ctxs;
        {
            RWMutexType::WriteLock lock(m_mutex);
            ctxs.swap(m_ctxs);
        }
        {
            RWMutexType::WriteLock lock(m_queueMutex);
            m_queue.clear();
        }
        for (const auto& i : ctxs) {
            i.second->result = IO_ERROR;
            i.second->doRsp();
        }
        return true;
    }

    void AsyncSocketStream::doRead() {
        try {
            while (isConnected()) {
                auto ctx = doRecv();
                if (ctx) {
                    ctx->doRsp();
                }
            }
        } catch (...) {
        }

        innerClose();
        m_waitsem.notify();
    }

    void AsyncSocketStream::doWrite() {
        try {
            while (isConnected()) {
                m_sem.wait();
                std::list<Ctx::ptr> ctxs;
                {
                    RWMutexType::WriteLock lock(m_queueMutex);
                    m_queue.swap(ctxs);
                }
                auto self = shared_from_this();
                for (const auto& i : ctxs) {
                    i->doSend(self);
                }
            }
        } catch (...) {
        }
        {
            RWMutexType::WriteLock lock(m_queueMutex);
            m_queue.clear();
        }
        m_waitsem.notify();
    }

    void AsyncSocketStream::startRead() {
        m_iomanager->schedule(std::bind(&AsyncSocketStream::doRead, shared_from_this()));
    }

    void AsyncSocketStream::startWrite() {
        m_iomanager->schedule(std::bind(&AsyncSocketStream::doWrite, shared_from_this()));
    }

    void AsyncSocketStream::onTimeOut(Ctx::ptr ctx) {
        {
            RWMutexType::WriteLock lock(m_mutex);
            m_ctxs.erase(ctx->sn);
        }
        ctx->timed = true;
        ctx->doRsp();
    }

    bool AsyncSocketStream::start() {
        if (!m_iomanager) {
            m_iomanager = sylar::IOManager::GetThis();
        }

        do {
            waitFiber();
            if (m_timer) {
                m_timer->cancel();
                m_timer = nullptr;
            }

            if (!isConnected()) {
                if (!m_socket->reconnect()) {
                    innerClose();
                    m_waitsem.notify();
                    m_waitsem.notify();
                    break;
                }
            }

            if (m_connectCb) {
                if (!m_connectCb(shared_from_this())) {
                    innerClose();
                    m_waitsem.notify();
                    m_waitsem.notify();
                    break;
                }
            }

            startRead();
            startWrite();
            return true;
        } while (false);
        if (m_autoConnect) {
            if (m_timer) {
                m_timer->cancel();
                m_timer = nullptr;
            }
            m_timer = m_iomanager->addTimer(2 * 1000,
                    std::bind(&AsyncSocketStream::start, shared_from_this()));
        }
        return false;
    }

    AsyncSocketStream::Ctx::ptr AsyncSocketStream::getCtx(uint32_t sn) {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_ctxs.find(sn);
        return it != m_ctxs.end() ? it->second : nullptr;
    }

    AsyncSocketStream::Ctx::ptr AsyncSocketStream::getAndDelCtx(uint32_t sn) {
        Ctx::ptr ctx;
        RWMutexType::WriteLock lock(m_mutex);
        auto it = m_ctxs.find(sn);
        if (it != m_ctxs.end()) {
            ctx = it->second;
            m_ctxs.erase(it);
        }
        return ctx;
    }

    AsyncSocketStream::Ctx::Ctx()
    : sn(0),
    timeout(0),
    result(0),
    timed(false),
    scheduler(nullptr) {
    }

    void AsyncSocketStream::Ctx::doRsp() {
        Scheduler* sch = scheduler;
        if (!sylar::Atomic::compareAndSwapBool(scheduler, sch, (Scheduler*)nullptr)) {
            return;
        }
        if (!sch || !fiber) {
            return;
        }
        if (timer) {
            timer->cancel();
            timer = nullptr;
        }
        if (timed) {
            result = TIMEOUT;
        }
        sch->schedule(&fiber);
    }

    bool AsyncSocketStream::addCtx(Ctx::ptr ctx) {
        RWMutexType::WriteLock lock(m_mutex);
        m_ctxs.insert(std::make_pair(ctx->sn, ctx));
        return true;
    }

    bool AsyncSocketStream::enqueue(AsyncSocketStream::Ctx::ptr ctx) {
        RWMutexType::WriteLock lock(m_mutex);
        bool empty = m_queue.empty();
        m_queue.push_back(ctx);
        lock.unlock();
        if (empty) {
            m_sem.notify();
        }
        return empty;
    }

    void AsyncSocketStream::close() {
        m_autoConnect = false;
        SchedulerSwitcher ss(m_iomanager);
        SocketStream::close();
    }

    AsyncSocketStreamManager::AsyncSocketStreamManager()
    : m_size(0),
    m_idx(0) {
    }

    void AsyncSocketStreamManager::add(AsyncSocketStream::ptr stream) {
        RWMutexType::WriteLock lock(m_mutex);
        m_datas.push_back(stream);
        ++m_size;
        if (m_connectCb) {
            stream->setConnectCb(m_connectCb);
        }

        if (m_disconnectCb) {
            stream->setDisConnectCb(m_disconnectCb);
        }
    }

    void AsyncSocketStreamManager::clear() {
        RWMutexType::WriteLock lock(m_mutex);
        for (const auto& i : m_datas) {
            i->close();
        }
        m_datas.clear();
        m_size = 0;
    }

    void AsyncSocketStreamManager::setConnection(const std::vector<AsyncSocketStream::ptr>& streams) {
        auto cs = streams;
        RWMutexType::WriteLock lock(m_mutex);
        cs.swap(m_datas);
        m_size = m_datas.size();
        if (m_connectCb || m_disconnectCb) {
            for (const auto& i : m_datas) {
                if (m_connectCb) {
                    i->setConnectCb(m_connectCb);
                }
                if (m_disconnectCb) {
                    i->setDisConnectCb(m_disconnectCb);
                }
            }
        }
        lock.unlock();

        for (const auto& i : cs) {
            i->close();
        }
    }

    AsyncSocketStream::ptr AsyncSocketStreamManager::get() {
        RWMutexType::ReadLock lock(m_mutex);
        for (uint32_t i = 0; i < m_size; i++) {
            auto idx = sylar::Atomic::addFetch(m_idx, 1);
            if (m_datas[idx % m_size]->isConnected()) {
                return m_datas[idx % m_size];
            }
        }
        return nullptr;
    }

    void AsyncSocketStreamManager::setConnectCb(connect_callback v) {
        m_connectCb = v;
        RWMutexType::WriteLock lock(m_mutex);
        for (const auto& i : m_datas) {
            i->setConnectCb(m_connectCb);
        }
    }

    void AsyncSocketStreamManager::setDisConnectCb(disconnect_callback v) {
        m_disconnectCb = v;
        RWMutexType::WriteLock lock(m_mutex);
        for (const auto&i : m_datas) {
            i->setDisConnectCb(m_disconnectCb);
        }
    }
}
