#ifndef __SOCKS_HH__
#define __SOCKS_HH__

#include <memory>
#include "my_sylar/stream.hh"
#include "my_sylar/ns/ares.hh"

namespace sylar {

Stream::ptr tunnel(sylar::Stream::ptr cstream, AresChannel::ptr channel, const std::string& targetIP = "",
          uint16_t targetPort = 0);
}

#endif

