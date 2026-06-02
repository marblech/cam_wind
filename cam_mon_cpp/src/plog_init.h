#ifndef CAM_WIND_SRC_PLOG_INIT_H
#define CAM_WIND_SRC_PLOG_INIT_H

/**
 * @file src/plog_init.h
 * @brief plog 日志初始化封装
 * @author marblech
 * @date 2026-06-02
 * @copyright SPDX: MIT OR as-project-specifies
 *
 * 提供一个简单接口用于初始化 plog 日志系统，
 * 使用滚动文件策略（示例：2MB 每个文件，最多 5 个文件）。
 */

#include <plog/Log.h>
#include "cammon_api.h"

/**
 * @brief 初始化 plog 日志，使用滚动文件（2MB/文件，最多5个）
 */
CAMMON_API void initPlog();

#endif /* CAM_WIND_SRC_PLOG_INIT_H */
