/**************************************************************************
* 版权所有(C), 2026，海华电子企业（中国）有限公司
* 文件名：	plog_init.cpp
* 作  者：	陈斌
* 版  本：	v1.0
* 日  期：	2026/07/16
* 文件描述:	PLOG 初始化，设置日志输出等级与滚动文件 Appender
* 函数列表：
* 		CAMMON_API void initPlog() - 初始化 plog 日志库，设置输出等级与记录器
* 修改历史:
* 修改日期：2026/07/16
* 修改者：自动化修改
* 修改内容：将 PLOG 输出等级改为 ERROR，添加文件头与函数注释
***************************************************************************/

#include "plog_init.h"
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>


// 使用 TxtFormatter 写入文本日志，文件名 cammon.log
static plog::RollingFileAppender<plog::TxtFormatter> rollingAppender("cammon.log", 2 * 1024 * 1024, 5);

/**
 * @brief 初始化 plog 日志库，并设置日志输出等级与 appender
 *
 * @return void
 */
CAMMON_API void initPlog() 
{
    // 将日志等级设置为 ERROR
    plog::init(plog::error, &rollingAppender);
}
