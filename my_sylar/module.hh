#ifndef __MODULE_HH__
#define __MODULE_HH__

#include <memory>
#include <string>
#include <map>
#include <vector>

#include "stream.hh"
#include "thread.hh"

namespace sylar {
    class Module {
    public:
        typedef std::shared_ptr<Module> ptr;
        Module(const std::string& name,
                const std::string& version, const std::string& filename);
        virtual ~Module() {};
        virtual void onBeforeArgsParse(int argc, char** argv);
        virtual void onAfterArgsParse(int argc, char** argv);
        virtual bool onLoad();
        virtual bool onUnload();
        virtual bool onConnect(sylar::Stream::ptr stream);
        virtual bool onDisconnect(sylar::Stream::ptr stream);
        virtual bool onServerReady();
        virtual bool onServerUp();
        virtual std::string statusString();

        const std::string& getName() const { return m_name; }
        const std::string& getVersion() const { return m_version; }
        const std::string& getFilename() const { return m_filename; }
        const std::string& getId() const { return m_id; }
        void setFilename(const std::string& file) { m_filename = file; }

    protected:
        std::string     m_name;
        std::string     m_version;
        std::string     m_filename;
        std::string     m_id;
    };

    class Library {
    public:
        static Module::ptr GetModule(const std::string& path);
    };

    class ModuleManager {
    public:
        typedef std::shared_ptr<ModuleManager> ptr;
        typedef RWMutex RWMutexType;
        void add(Module::ptr m);
        void del(const std::string& m);
        void delAll();
        void init();
        Module::ptr getModule(const std::string& name);
        void onConnect(sylar::Stream::ptr stream);
        void onDisconnect(sylar::Stream::ptr stream);
        void listAll(std::vector<Module::ptr>& ms);
        static ModuleManager* GetModuleMgr();

    private:
        void initModule(const std::string& path);
    private:
        ModuleManager();
        static ModuleManager*   m_mmgr;
        RWMutexType             m_mutex;
        std::map<std::string, Module::ptr>  m_modules;
    };
}

#endif
