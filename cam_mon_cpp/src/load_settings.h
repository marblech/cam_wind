#ifndef LOAD_SETTINGS_H
#define LOAD_SETTINGS_H
#include <iostream>
#include <vector>

// #include <camInfoHelper.h>

/**
 * 读取配置文件方法
 * 
 * create by: 陈斌
*/
int set_conf_file(std::string &&);

std::string get_conf_file();

std::string loadSetting(std::string &&,std::string &&);

// std::vector<cam_control_interface::caminfowrap> load_multi_cam_settings(int);

#endif // LOAD_SETTINGS_H
