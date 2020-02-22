#include "http/http_connection.hh"
#include "http/http_parser.hh"
#include "log.hh"

namespace sylar {
    namespace http {
        static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
        HttpConnection::HttpConnection(Socket::ptr sock, bool owner)
        : SocketStream(sock, owner) {
        }

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
    }
}