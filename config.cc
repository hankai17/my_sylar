#include "config.hh"
#include <list>

namespace sylar {

    Config::ConfigVarMap Config::m_config; //!!!

    ConfigVarBase::ptr Config::LookupBase(const std::string &name) { // It just a simple judge
        auto it = Config::getMap().find(name);
        return it == Config::getMap().end() ? nullptr : it->second; // Not use m_data[name]
    }

    static void ListAllMember(const std::string &prefix, const YAML::Node &node,
                              std::list<std::pair<std::string, const YAML::Node> > &all_nodes) {
        if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Config invalid name: " << prefix;
            return;
        }
        all_nodes.push_back(std::make_pair(prefix, node));
        if (node.IsMap()) {
            for (auto &it : node) { // const auto& it
                ListAllMember(prefix.empty() ? it.first.Scalar() : prefix + "." + it.first.Scalar(), it.second,
                              all_nodes);
            }
        }
    }

    void Config::loadFromYaml(const YAML::Node &root) {
        std::list<std::pair<std::string, const YAML::Node> > all_nodes;
        ListAllMember("", root, all_nodes);

        for (auto &i : all_nodes) { // for (auto i : all_nodes)
            std::string key = i.first;
            if (key.empty()) {
                continue;
            }
            //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "key: " << key;

            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            ConfigVarBase::ptr var = LookupBase(key);
            if (var) {
                if (i.second.IsScalar()) {
                    var->fromString(i.second.Scalar());
                    //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "key: " << key << " string: " << i.second.Scalar();
                } else {
                    std::stringstream ss;
                    ss << i.second;
                    //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "key: " << key << " string: " << ss.str();
                    var->fromString(ss.str());
                }
            } else {
                //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "not exists this key: " << key << " string: " << i.second;
            }
        }
    }

    sylar::ConfigVar<std::vector<sylar::LoggerConfig> >::ptr g_log_config =
            sylar::Config::Lookup("logs", std::vector<sylar::LoggerConfig>{LoggerConfig("root")}, "sylar log");

    /*
    g_log_config->addListener("sylar log", [](const std::vector<sylar::LoggerConfig>& old_val, //Can not call it directly
            const std::vector<sylar::LoggerConfig>& new_val) {
    });
     */

    struct LogConfigIniter {
        bool is_in_(const sylar::LoggerConfig &a, const std::vector<sylar::LoggerConfig> &b) { //is a in b?
            for (const auto &i : b) {
                if (a.getLogName() == i.getLogName()) {
                    return true;
                }
            }
            return false;
        }

        bool is_appender_in_(const sylar::LogAppender::ptr &a, const std::vector<sylar::LogAppender::ptr> &b) {
            for (const auto &i : b) {
                if (a->getType() == i->getType()) { //&& a->getFile() == i->getFile()) {
                    return true;
                }
            }
            return false;
        }

        LogConfigIniter() {
            g_log_config->addListener("logs", [this](const std::vector<sylar::LoggerConfig> &old_val, // Must have this
                                                     const std::vector<sylar::LoggerConfig> &new_val) {
                SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "logs on_change_callback";
                for (const auto &i : new_val) {
                    if (is_in_(i, old_val)) {
                        sylar::Logger::ptr logger = SYLAR_LOG_NAME(i.getLogName());
                        if (i.getLogLevel() != logger->getLevel()) {
                            logger->setLevel(i.getLogLevel());
                        }
                        if (i.getFormatter() != logger->getFormatter()->getFormatter()) {
                            logger->getFormatter()->setFormatter(i.getFormatter());
                        }

                        bool new_appender_flag = 0;
                        for (const auto &j : i.getAppenders()) { //
                            //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << logger->toYamlString();

                            //if old appender not in new we should del it
                            for (const auto &ii : logger->getAppender()) {
                                //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << ii->toYamlString();
                                //SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << i.toYamlString();
                                if (!is_appender_in_(ii, i.getAppenders())) {
                                    logger->delAppender(ii);
                                }
                            }

                            new_appender_flag = 1;
                            for (const auto &jj : logger->getAppender()) {
                                if (jj->getType() == j->getType()) {
                                    new_appender_flag = 0;
                                    if (jj->getFile() != j->getFile()) {
                                        jj->setFile(j->getFile());
                                    }
                                }
                            }
                            if (new_appender_flag == 1) {
                                sylar::LogAppender::ptr ap;
                                if (j->getFile() == "") {
                                    ap = std::make_shared<sylar::StdoutLogAppender>();
                                } else {
                                    ap = std::make_shared<sylar::FileLogAppender>(j->getFile());
                                }
                                ap->setType(j->getType());
                                ap->setFile(j->getFile());
                                logger->addAppender(ap);
                            }

                        }

                    } else { //new logger
                        sylar::Logger::ptr logger = SYLAR_LOG_NAME(i.getLogName());
                        logger->setName(i.getLogName());
                        logger->setLevel(i.getLogLevel());
                        logger->getFormatter()->setFormatter(i.getFormatter());
                        for (const auto &j : i.getAppenders()) {
                            sylar::LogAppender::ptr ap;
                            if (j->getFile() == "") {
                                ap = std::make_shared<sylar::StdoutLogAppender>();
                            } else {
                                ap = std::make_shared<sylar::FileLogAppender>(j->getFile());
                            }
                            ap->setType(j->getType());
                            ap->setFile(j->getFile());
                            logger->addAppender(ap);
                        }
                    }
                }
                /*
                for (auto it = old_val.begin(); it != old_val.end();) {
                        if (!is_in_(*it, new_val)) {
                            //del the old
                              it = old_val.erase(it);
                        } else {
                            it++;
                        }
                }
                 */

                for (const auto &i : old_val) {
                    if (!is_in_(i, new_val)) {
                        //del the old
                        sylar::Logger::ptr logger = SYLAR_LOG_NAME(i.getLogName());
                        logger->setLevel(sylar::LogLevel::Level(100));
                        logger->clearAppender();
                    }
                }

            });
        }

    };

    static LogConfigIniter __log_config_init;
}