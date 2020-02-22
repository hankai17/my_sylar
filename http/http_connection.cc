#include "http/http_connection.hh"
#include "http/http_parser.hh"

namespace sylar {
    namespace http {
        HttpConnection::HttpConnection(Socket::ptr sock, bool owner)
        : SocketStream(sock, owner) {
        }

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
                offset = len - nparsed;
                if (offset == (int)buff_size) {
                    close();
                    return nullptr;
                }
                if (parser->isFinished()) {
                    break;
                }
            } while (true);
            auto& client_parser = parser->getParser();
            if (client_parser.chunked) { // https://blog.csdn.net/u014558668/article/details/70141956
                std::string body;



            } else {
                int64_t length = parser->getContentLength();
                if (length > 0) {
                    std::string body;
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
                    parser->getData()->setBody(body);
                }
            }
            return parser->getData();
        }

        int HttpConnection::sendRequest(HttpRequest::ptr req) {
            return writeFixSize(req->toString().c_str(), req->toString().size());
        }
    }
}