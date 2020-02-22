#ifndef __HTTP_CONNECTION_HH__
#define __HTTP_CONNECTION_HH__

#include <memory>
#include "stream.hh"
#include "socket.hh"
#include "http/http.hh"

namespace sylar {
    namespace http {
        class HttpConnection : public SocketStream {
        public:
            typedef std::shared_ptr<HttpConnection> ptr;
            HttpConnection(Socket::ptr sock, bool owner = true);
            HttpResponse::ptr recvResponse();
            int sendRequest(HttpRequest::ptr req);
        };
    }
}

#endif