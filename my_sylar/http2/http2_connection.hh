#ifndef __HTTP2_CONNECTION_H__
#define __HTTP2_CONNECTION_H__

#include "my_sylar/http2/frame.h"
#include "my_sylar/http2/hpack.h"

namespace sylar {
namespace http2 {

class Http2Connection : public AsyncSocketStream {
public:
};

}
}

#endif

