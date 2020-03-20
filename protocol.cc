#include "protocol.hh"
#include "log.hh"
#include "endian.hh"
#include "config.hh"
#include <sstream>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    ByteArray::ptr Message::toByteArray() {
        ByteArray::ptr ba(new ByteArray);
        if (serializeToByteArray(ba)) {
            return ba;
        }
        return nullptr;
    }

    Request::Request()
    : m_sn(0),
    m_cmd(0) {
    }

    bool Request::serializeToByteArray(ByteArray::ptr byte_array) {
        byte_array->writeFint8(getType());
        byte_array->writeUint32(m_sn);
        byte_array->writeUint32(m_cmd);
        return true;
    }

    bool Request::parseFromByteArray(ByteArray::ptr byte_array) {
        m_sn = byte_array->readUint32();
        m_cmd = byte_array->readUint32();
        return true;
    }

    Response::Response()
            : m_sn(0),
              m_cmd(0),
              m_result(0),
              m_resultStr() {
    }

    bool Response::serializeToByteArray(ByteArray::ptr byte_array) {
        byte_array->writeFint8(getType());
        byte_array->writeUint32(m_sn);
        byte_array->writeUint32(m_cmd);
        byte_array->writeUint32(m_result);
        byte_array->writeStringVint(m_resultStr);
        return true;
    }

    bool Response::parseFromByteArray(ByteArray::ptr byte_array) {
        m_sn = byte_array->readUint32();
        m_cmd = byte_array->readUint32();
        m_result = byte_array->readUint32();
        m_resultStr = byte_array->readStringVint();
        return true;
    }


    Notify::Notify()
    : m_notify(0) {
    }

    bool Notify::serializeToByteArray(ByteArray::ptr byte_array) {
        byte_array->writeFint8(getType());
        byte_array->writeUint32(m_notify);
        return true;
    }

    bool Notify::parseFromByteArray(ByteArray::ptr byte_array) {
        m_notify = byte_array->readUint32();
        return true;
    }

}