#pragma once
#include <plog/Log.h>
#include "cammon_api.h"
// 初始化 plog 日志，使用滚动文件，2MB 每个文件，最多 5 个
CAMMON_API void initPlog();
