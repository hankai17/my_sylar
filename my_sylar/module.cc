#include "module.hh"
#include "log.hh"
#include "my_sylar/application.hh"
#include "my_sylar/config.hh"
#include "util.hh"

#include <vector>
#include <dlfcn.h>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static sylar::ConfigVar<std::string>::ptr g_module_path =
            Config::Lookup("module.path", std::string("module"), "module path");

    Module::Module(const std::string& name,
           const std::string& version,
           const std::string& filename)
           : m_name(name),
           m_version(version),
           m_filename(filename) {
    }

    void Module::onBeforeArgsParse(int argc, char** argv) {
        return;
    }

    void Module::onAfterArgsParse(int argc, char** argv) {
        return;
    }

    bool Module::onLoad() {
        return false;
    }

    bool Module::onUnload() {
        return false;
    }

    bool Module::onConnect(sylar::Stream::ptr stream) {
        return false;
    }

    bool Module::onDisconnect(sylar::Stream::ptr stream) {
        return false;
    }

    bool Module::onServerReady() {
        return false;
    }

    bool Module::onServerUp() {
        return false;
    }

    typedef Module* (*create_module)();
    typedef void (*destroy_module)(Module*);

    class ModuleCloser {
    public:
        ModuleCloser(void* handle, destroy_module d)
        : m_handle(handle),
        m_destroy(d) {
        }
        void operator() (Module* module) {
            std::string name = module->getName();
            std::string version = module->getVersion();
            std::string path = module->getFilename();
            m_destroy(module);
            int ret = dlclose(m_handle);
            if (ret) {
                SYLAR_LOG_ERROR(g_logger) << "dlclose handle failed, "
                << " handle: " << m_handle
                << " name: " << name << " version: " << version
                << " path: " << path << " error: " << dlerror();
            } else {
                SYLAR_LOG_DEBUG(g_logger) << "dlclose handle success, "
                                          << " handle: " << m_handle
                                          << " name: " << name << " version: " << version
                                          << " path: " << path;
            }
        }
    private:
        void*       m_handle;
        destroy_module m_destroy;
    };

    Module::ptr Library::GetModule(const std::string& path) {
        void* handle = dlopen(path.c_str(), RTLD_NOW);
        if (!handle) {
            SYLAR_LOG_ERROR(g_logger) << "Can not load library path: "
            << path << " error: " << dlerror();
            return nullptr;
        }

        create_module create = (create_module)dlsym(handle, "CreateModule");
        if (!create) {
            SYLAR_LOG_ERROR(g_logger) << "Can not laod symbol_CreateModule in: "
                                      << path << " error: " << dlerror();
            dlclose(handle);
            return nullptr;
        }

        destroy_module destroy = (destroy_module)dlsym(handle, "DestroyModule");
        if (!destroy) {
            SYLAR_LOG_ERROR(g_logger) << "Can not laod symbol_DestroyModule in: "
                                      << path << " error: " << dlerror();
            dlclose(handle);
            return nullptr;
        }
        Module::ptr module(create(), ModuleCloser(handle, destroy));
        module->setFilename(path);
        SYLAR_LOG_DEBUG(g_logger) << "load module success name: " << module->getName()
        << " version: " << module->getVersion()
        << " path: " << module->getFilename();
        Config::loadFromConfDir(sylar::Env::getEnvr()->getConfPath(), true);
        return module;
    }

    std::string Module::statusString() {
        return "";
    }

    ModuleManager* ModuleManager::m_mmgr = new ModuleManager();

    ModuleManager::ModuleManager() {}

    ModuleManager* ModuleManager::GetModuleMgr() {
        return m_mmgr;
    }

    void ModuleManager::add(Module::ptr m) {
        del(m->getId());
        RWMutexType::WriteLock lock(m_mutex);
        m_modules[m->getId()] = m;
    }

    void ModuleManager::del(const std::string& m) {
        RWMutexType::WriteLock lock(m_mutex);
        auto it = m_modules.find(m);
        if (it == m_modules.end()) {
            return;
        }
        Module::ptr tmp_mod = it->second;
        m_modules.erase(it);
        lock.unlock();
        tmp_mod->onUnload();
    }

    void ModuleManager::delAll() {
        RWMutexType::ReadLock lock(m_mutex);
        auto tmp = m_modules;
        lock.unlock();

        for (auto& i : m_modules) {
            del(i.first);
        }
    }

    void ModuleManager::init() {
        auto path = sylar::Env::getEnvr()->getAbsolutPath(g_module_path->getValue());
        std::vector<std::string> files;
        sylar::FSUtil::ListAllFile(files, path, ".so");
        std::sort(files.begin(), files.end());
        for (auto& i : files) {
            initModule(i);
        }
    }

    void ModuleManager::onConnect(sylar::Stream::ptr stream) {
        std::vector<Module::ptr> ms;
        listAll(ms);
        for (auto& m : ms) {
            m->onConnect(stream);
        }
    }

    void ModuleManager::onDisconnect(sylar::Stream::ptr stream) {
        std::vector<Module::ptr> ms;
        listAll(ms);
        for (auto& m : ms) {
            m->onDisconnect(stream);
        }
    }

    void ModuleManager::listAll(std::vector<Module::ptr>& ms) {
        RWMutexType::ReadLock lock(m_mutex);
        for (const auto& i : m_modules) {
            ms.push_back(i.second);
        }
    }

    void ModuleManager::initModule(const std::string& path) {
        Module::ptr m = Library::GetModule(path);
        if (m) {
            add(m);
        }
    }
}