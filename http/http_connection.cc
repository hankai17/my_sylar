#include "http/http_connection.hh"
#include "http/http_parser.hh"
#include "log.hh"

namespace sylar {
    namespace http {
        static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

        std::string HttpResult::toString() const {
            std::stringstream ss;
            ss << "[HttpResult: " << result
            << " error: " << error
            << " response: " << (response ? response->toString() : "nullptr")
            << "]";
            return ss.str();
        }

        HttpConnection::HttpConnection(Socket::ptr sock, bool owner)
        : SocketStream(sock, owner) {
        }

        HttpConnection::~HttpConnection() {}

        // http_server好写 好封装 因为只需要接受请求就可以了 请求头比较短 也好处理
        // 相反http_connection就不好封装了 接收到的数据有长有短 不好处理
        HttpResponse::ptr HttpConnection::recvResponse() {
            HttpResponseParser::ptr parser(new HttpResponseParser);
            uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
            std::shared_ptr<char> buffer(new char[buff_size + 1], [](char* ptr) {
                delete[] ptr;
            });
            char* data = buffer.get();
            int offset = 0;
            do {
                int len = read(data + offset, buff_size - offset);
                if (len <= 0) {
                    close();
                    return nullptr;
                }
                len += offset;
                data[len] = '\0';
                size_t nparsed = parser->execute(data, len, false);
                if (parser->hasError()) {
                    close();
                    return nullptr;
                }
                offset = len - nparsed; // 最后一次解析header后 offset等于当前data里内容的长度
                if (offset == (int)buff_size) {
                    close();
                    return nullptr;
                }
                if (parser->isFinished()) {
                    break;
                }
            } while (true);
            auto& client_parser = parser->getParser();
            std::string body;
            if (client_parser.chunked) { // https://blog.csdn.net/u014558668/article/details/70141956
                int len = offset;
                do { // 先拿到每个块的长度 再readFixLen
                    bool begin = true;
                    do {
                        if(!begin || len == 0) {
                            int rt = read(data + len, buff_size - len);
                            if(rt <= 0) {
                                close();
                                return nullptr;
                            }
                            len += rt;
                        }
                        data[len] = '\0';
                        size_t nparse = parser->execute(data, len, true); // 得到一个块的长度 // parsed通常很小
                        if(parser->hasError()) {
                            close();
                            return nullptr;
                        }
                        len -= nparse;
                        if(len == (int)buff_size) {
                            close();
                            return nullptr;
                        }
                        begin = false;
                    } while(!parser->isFinished());
                    //len -= 2;
                    SYLAR_LOG_DEBUG(g_logger) << "content_len=" << client_parser.content_len;
                    if(client_parser.content_len + 2 <= len) {
                        body.append(data, client_parser.content_len);
                        memmove(data, data + client_parser.content_len + 2
                                , len - client_parser.content_len - 2);
                        len -= client_parser.content_len + 2;
                    } else {
                        body.append(data, len);
                        int left = client_parser.content_len - len + 2;
                        while(left > 0) {
                            int rt = read(data, left > (int)buff_size ? (int)buff_size : left);
                            if(rt <= 0) {
                                close();
                                return nullptr;
                            }
                            body.append(data, rt);
                            left -= rt;
                        }
                        body.resize(body.size() - 2);
                        len = 0;
                    }
                } while(!client_parser.chunks_done);
            } else {
                int64_t length = parser->getContentLength();
                if (length > 0) {
                    body.resize(length);
                    int len = 0;
                    if (length >= offset) {
                        memcpy(&body[0], data, offset);
                        len = offset;
                    } else {
                        memcpy(&body[0], data, length);
                        len = length;
                    }
                    length -= offset;
                    if (length > 0) {
                        if (readFixSize(&body[len], length) <= 0) {
                            close();
                            return nullptr;
                        }
                    }
                    //parser->getData()->setBody(body);
                }
            }
            parser->getData()->setBody(body);
            return parser->getData();
        }

        int HttpConnection::sendRequest(HttpRequest::ptr req) {
            return writeFixSize(req->toString().c_str(), req->toString().size());
        }


        HttpResult::ptr HttpConnection::DoGet(const std::string& url, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            Uri::ptr uri = Uri::Create(url);
            if (!uri) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL, nullptr, "invalid url: " + url);
            }
            return DoGet(uri, timeout_ms, headers, body);
        }

        HttpResult::ptr HttpConnection::DoGet(Uri::ptr uri, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            return DoRequest(HttpMethod::GET, uri, timeout_ms, headers, body);
        }

        HttpResult::ptr HttpConnection::DoPost(const std::string& url, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            Uri::ptr uri = Uri::Create(url);
            if (!uri) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL, nullptr, "invalid url: " + url);
            }
            return DoPost(uri, timeout_ms, headers, body);
        }

        HttpResult::ptr HttpConnection::DoPost(Uri::ptr uri, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            return DoRequest(HttpMethod::POST, uri, timeout_ms, headers, body);
        }

        HttpResult::ptr HttpConnection::DoRequest(HttpMethod method, const std::string& url, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            Uri::ptr uri = Uri::Create(url);
            if (!uri) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL, nullptr, "invalid url: " + url);
            }
            return DoRequest(method, uri, timeout_ms, headers, body);
        }

        HttpResult::ptr HttpConnection::DoRequest(HttpMethod method, Uri::ptr uri, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            HttpRequest::ptr req = std::make_shared<HttpRequest>();
            req->setPath(uri->getPath());
            req->setQuery(uri->getQuery());
            req->setFragment(uri->getFragment());
            req->setMethod(method);
            bool has_host = false;
            for (const auto& i : headers) {
                if (strcasecmp(i.first.c_str(), "connection") == 0) {
                    if (strcasecmp(i.second.c_str(), "keep-alive") == 0) {
                        req->setClose(false);
                    }
                    continue;
                }
                if (!has_host && strcasecmp(i.first.c_str(), "host") == 0) {
                    has_host = !i.second.empty();
                }
                req->setHeader(i.first, i.second);
            }
            if (!has_host) {
                req->setHeader("Host", uri->getHost());
            }
            req->setBody(body);
            return DoRequest(req, uri, timeout_ms);
        }

        HttpResult::ptr HttpConnection::DoRequest(HttpRequest::ptr req, Uri::ptr uri, uint64_t timeout_ms) {
            Address::ptr addr = uri->createAddress(); // ip in uri !!!
            if (!addr) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_HOST, nullptr,
                        "invalid host: " + uri->getHost());
            }
            Socket::ptr sock = Socket::CreateTCP(addr);
            if (!sock) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::CREATE_SOCKET_ERROR, nullptr,
                        "create socket failed: " + addr->toString()
                        + " errno: " + std::to_string(errno) + " strerrno: " + std::string(strerror(errno)));
            }
            if (!sock->connect(addr)) { // timeout_ms not connect timeout
                return std::make_shared<HttpResult>((int)HttpResult::Error::CONNECT_FAIL, nullptr,
                        "connect faild: ", addr->toString());
            }
            sock->setRecvTimeout(timeout_ms);
            HttpConnection::ptr conn = std::make_shared<HttpConnection>(sock);
            int ret = conn->sendRequest(req);
            if (ret == 0) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_CLOSED_BY_PEER, nullptr,
                        "send request closed by peer: ", addr->toString());
            }
            if (ret < 0) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_SOCKET_ERROR, nullptr,
                        "send request socket errno: " + std::to_string(errno) + " strerror: " + std::string(strerror(errno)));
            }

            auto resp = conn->recvResponse();
            if (!resp) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::TIMEOUT, nullptr,
                        "may be timeout & peer closed: " + addr->toString() + " timeout_ms: " + std::to_string(timeout_ms));
            }
            return std::make_shared<HttpResult>((int)HttpResult::Error::OK, resp, "ok");
        }

        HttpConnectionPool::HttpConnectionPool(const std::string& host, const std::string& vhost,
                           uint32_t port, uint32_t maxsize, uint32_t maxalivetime, uint32_t maxreq)
                           : m_host(host),
                           m_vhost(vhost),
                           m_port(port),
                           m_maxSize(maxsize),
                           m_maxAliveTime(maxalivetime),
                           m_maxRequest(maxreq) {
        }

        HttpConnection::ptr HttpConnectionPool::getConnection() {
            uint64_t now_ms = sylar::GetCurrentMs();
            std::vector<HttpConnection*> invalid_conns;
            HttpConnection* ptr = nullptr;
            MutexType::Lock lock(m_mutex);
            while (!m_conns.empty()) {
                auto conn = *m_conns.begin();
                m_conns.pop_front();
                if (!conn->isConnected()) {
                    invalid_conns.push_back(conn);
                    continue;
                }
                if ((conn->m_createTime + m_maxAliveTime) > now_ms) {
                    invalid_conns.push_back(conn);
                    continue;
                }
                ptr = conn;
                break;
            }

            lock.unlock();
            for (const auto& i : invalid_conns) {
                delete i;
            }
            m_total -= invalid_conns.size();
            if (!ptr) {
                IPAddress::ptr addr = Address::LookupAnyIPAddress(m_host); // 这也太有局限性了吧 只是针对一个host的连接池
                if (!addr) {
                    SYLAR_LOG_ERROR(g_logger) << "get addr failed, m_host: " << m_host;
                    return nullptr;
                }
                addr->setPort(m_port);
                Socket::ptr sock = Socket::CreateTCP(addr);
                if (!sock) {
                    SYLAR_LOG_ERROR(g_logger) << "createtcp faild: " << addr->toString();
                    return nullptr;
                }
                if (!sock->connect(addr)) {
                    SYLAR_LOG_ERROR(g_logger) << "sock connect faild" << addr->toString();
                    return nullptr;
                }
                ptr = new HttpConnection(sock);
                ++m_total;
            }
            return HttpConnection::ptr(ptr, std::bind(&HttpConnectionPool::ReleasePtr,
                    std::placeholders::_1, this));
        }

        void HttpConnectionPool::ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool) { // 好屌的思路
            ++ptr->m_request;
            if (!ptr->isConnected()
            || (ptr->m_createTime + pool->m_maxAliveTime >= sylar::GetCurrentMs())
            || (ptr->m_request >= pool->m_maxRequest)) {
                delete ptr;
                --pool->m_total;
                return;
            }
            MutexType::Lock lock(pool->m_mutex);
            pool->m_conns.push_back(ptr);
        }

        HttpResult::ptr HttpConnectionPool::doGet(const std::string& url, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            return doRequest(HttpMethod::GET, url, timeout_ms, headers, body);
        }

        HttpResult::ptr HttpConnectionPool::doGet(Uri::ptr uri, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            std::stringstream ss;
            ss << uri->getPath()
            << (uri->getQuery().empty() ? "" : "?")
            << uri->getQuery()
            << (uri->getFragment().empty() ? "" : "?")
            << uri->getFragment();
            return doGet(ss.str(), timeout_ms, headers, body);
        }

        HttpResult::ptr HttpConnectionPool::doPost(const std::string& url, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            return doRequest(HttpMethod::POST, url, timeout_ms, headers, body);
        }

        HttpResult::ptr HttpConnectionPool::doPost(Uri::ptr uri, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            std::stringstream ss;
            ss << uri->getPath()
               << (uri->getQuery().empty() ? "" : "?")
               << uri->getQuery()
               << (uri->getFragment().empty() ? "" : "?")
               << uri->getFragment();
            return doPost(ss.str(), timeout_ms, headers, body);
        }

        HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method, const std::string& url, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            HttpRequest::ptr req = std::make_shared<HttpRequest>();
            req->setPath(url);
            req->setMethod(method);
            bool has_host = false;
            for (const auto& i : headers) {
                if (strcasecmp(i.first.c_str(), "connection") == 0) {
                    if (strcasecmp(i.second.c_str(), "keep-alive") == 0) {
                        req->setClose(false);
                    }
                    continue;
                }
                if (!has_host && strcasecmp(i.first.c_str(), "host") == 0) {
                    has_host = !i.second.empty();
                }
                req->setHeader(i.first, i.second);
            }
            if (!has_host) {
                if (m_vhost.empty()) {
                    req->setHeader("Host", m_host)
                } else {
                    req->setHeader("Host", m_vhost);
                }
            }
            req->setBody(body);
            return doRequest(req, timeout_ms);
        }

        HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method, Uri::ptr uri, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers, const std::string& body) {
            std::stringstream ss;
            ss << uri->getPath()
               << (uri->getQuery().empty() ? "" : "?")
               << uri->getQuery()
               << (uri->getFragment().empty() ? "" : "?")
               << uri->getFragment();
            return doRequest(method, ss.str(), timeout_ms, headers, body);
        }

        HttpResult::ptr HttpConnectionPool::doRequest(HttpRequest::ptr req, uint64_t timeout_ms) {
            auto conn = getConnection();
            if (!conn) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::POOL_GET_CONN_FAIL, nullptr,
                        "pool host: " + m_host + " port: " + std::to_string(m_port));
            }
            Socket::ptr sock = conn->getSocket();
            if (!sock) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::POOL_INVAILD_CONN, nullptr,
                                                    "pool host: " + m_host + " port: " + std::to_string(m_port));
            }
            sock->setRecvTimeout(timeout_ms);
            int ret = conn->sendRequest(req);
            if (ret == 0) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_CLOSED_BY_PEER, nullptr,
                                                    "send request closed by peer: ", sock->getRemoteAddress()->toString());
            }
            if (ret < 0) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_SOCKET_ERROR, nullptr,
                                                    "send request socket errno: " + std::to_string(errno) +
                                                    " strerror: " + std::string(strerror(errno)));
            }

            auto resp = conn->recvResponse();
            if (!resp) {
                return std::make_shared<HttpResult>((int)HttpResult::Error::TIMEOUT, nullptr,
                                                    "may be timeout & peer closed: " + sock->getRemoteAddress()->toString() +
                                                    " timeout_ms: " + std::to_string(timeout_ms));
            }
            return std::make_shared<HttpResult>((int)HttpResult::Error::OK, resp, "ok");
        }

    }
}