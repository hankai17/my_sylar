#include "fiber.hh"

namespace sylar {
    static thread_local Fiber *t_fiber = nullptr;

    void Fiber::SetThis(sylar::Fiber *f) {
        t_fiber = f;
    }

    Fiber::ptr GetThis() {
    }

    Fiber::Fiber() {
        m_state = EXEC;
        SetThis(this);
    }
}