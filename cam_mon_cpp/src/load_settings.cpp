#include "load_settings.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>

std::string conf_file = "config.ini";

int set_conf_file(std::string &&config_file){
    conf_file = config_file;
    return 0;
}

std::string get_conf_file(){
    return conf_file;
}

std::string loadSetting(std::string &&tag_name,std::string &&param_name){
    if(!boost::filesystem::exists(get_conf_file())){
        std::cerr << "config.ini not exists." <<std::endl;
        return "";
    }
    boost::property_tree::ptree root_node,tag_system;
    boost::property_tree::ini_parser::read_ini(get_conf_file(),root_node);
    tag_system = root_node.get_child(tag_name);
    if(tag_system.count(param_name)!=1){
        std::cerr<<"reboot_cnt node not exists." <<std::endl;
        return "";
    }    
    std::string cnt = tag_system.get<std::string>(param_name);
    // std::cout<<"config:" <<cnt <<std::endl;
    return cnt;
}

