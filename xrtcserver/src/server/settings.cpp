#include "server/settings.h"

#include <iostream>

#include <yaml-cpp/yaml.h>

namespace xrtc {

bool Settings::Init(const char *confPath) {
    if (!confPath)  {
        fprintf(stderr, "filename is nullptr \n");
        return -1;
    }

    YAML::Node config = YAML::LoadFile(confPath);
    
    try {
        log_conf_.log_dir = config["log"]["log_dir"].as<std::string>();
        log_conf_.log_name = config["log"]["log_name"].as<std::string>();
        log_conf_.log_level = config["log"]["log_level"].as<std::string>();
        log_conf_.log_to_stderr = config["log"]["log_to_stderr"].as<bool>();

        ice_conf_.ice_min_port = config["ice"]["min_port"].as<int>();
        ice_conf_.ice_max_port = config["ice"]["max_port"].as<int>();

        signaling_server_options_.host_ip = config["signaling"]["host_ip"].as<std::string>();
        signaling_server_options_.port = config["signaling"]["port"].as<int>();
        signaling_server_options_.worker_num = config["signaling"]["worker_num"].as<int>(); 
        signaling_server_options_.connection_timeout = config["signaling"]["connection_timeout"].as<int>();

        rtc_server_options_.worker_num = config["rtc"]["worker_num"].as<int>();
        rtc_server_options_.candidate_ip = config["rtc"]["candidate_ip"].as<std::string>();

    } catch (YAML::Exception e) {
        fprintf(stderr, "catch a YAML::Exception, line: %d, column: %d"
        ", err:%s \n", e.mark.line + 1 , e.mark.column + 1, e.msg.c_str());
        return -1;
    }
    std::cout << "config file: " << log_conf_.log_dir << "/" << log_conf_.log_name << std::endl;

    return true;
}


} // end xrtc
