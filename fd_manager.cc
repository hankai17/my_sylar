#include "fd_manager.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace sylar {
    FdCtx::FdCtx(int fd)
    : m_isInit(false),
    m_isSocket(false),
    m_isClosed(false),
    m_userNonblock(false),
    m_sysNonblock(false),
    m_fd(fd),
    m_recvTimeout(-1),
    m_sendTimeout(-1) {
       init();
    }

    FdCtx::~FdCtx() {
    }

    bool FdCtx::init() {
        if (m_isInit) {
            return true;
        }
        m_recvTimeout = -1;
        m_sendTimeout = -1;

        struct stat fd_stat;
        if (-1 == fstat(m_fd, &fd_stat)) {
            m_isInit = false;
            m_isSocket = false;
        } else {
            m_isInit = true;
            m_isSocket = S_ISSOCK(fd_stat.st_mode);
        }

        if (m_isSocket) {
            int flags = fcntl_f(m_fd, F_GETFL, 0);
            if (!(flags | O_NONBLOCK)) {
                fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
            }
            m_sysNonblock = true;
        }
    }

}
