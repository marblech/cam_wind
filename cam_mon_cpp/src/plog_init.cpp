#include "plog_init.h"
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

// 使用 TxtFormatter 写入文本日志，文件名 cammon.log
static plog::RollingFileAppender<plog::TxtFormatter> rollingAppender("cammon.log", 2 * 1024 * 1024, 5);

CAMMON_API void initPlog() {
    plog::init(plog::info, &rollingAppender);
}
