# AGENTS.MD

目的：指导 agent 在为命令行工具（C/C++ 为主）生成源代码时，强制在文件头和所有接口/函数上使用 Doxygen 风格注释，并生成一致的 CLI 结构、帮助和示例。

## 全局约束
- 每个新生成的源/头文件顶部必须插入文件说明（file header）。
- 所有接口、函数、类/struct、命令处理函数必须使用 Doxygen 风格注释，至少包含 @brief、@param（如适用）、@return（如适用）。
- 注释语言为中文或中英对照，确保可用于自动化文档生成。

## 文件头模板
在每个文件开头插入以下模板并替换占位符：
/**
 * @file <FILENAME>
 * @brief <简短描述>
 * @author <作者>
 * @date <YYYY-MM-DD>
 * @copyright <版权声明或 SPDX>
 */

## 函数/接口注释模板
对于每个函数/公有方法：
/**
 * @brief <函数功能简短描述>
 * @param[in]  <name> <输入参数说明>
 * @param[out] <name> <输出参数说明>
 * @param[in,out] <name> <输入输出参数说明>
 * @return <返回值说明，异常或错误码>
 * @note <可选：备注、复杂度、线程安全性>

 ## 头文件保护（禁止使用 #pragma once）
- 严格禁止在任何头文件中使用 `#pragma once`。
- 必须使用传统的 include guard（#ifndef / #define / #endif）。
- 宏命名规则：
  - 使用文件路径和文件名生成宏（相对于仓库根或 include 根），全部大写。
  - 非字母数字字符替换为下划线（例如：/ -> _, . -> _）。
  - 可选后缀 `_H` 或 `_HPP`，例如：PROJECT_SUBDIR_HEADER_H 或 CLINE_CMD_HANDLER_H。
  - 示例格式： PROJECT_SUBDIR_FILENAME_H

### 示例
以下为头文件示例（file.h）：
```c
#ifndef PROJECT_SUBDIR_FILE_H
#define PROJECT_SUBDIR_FILE_H

/**
 * @file file.h
 * @brief 示例头文件
 */

... /* 头文件内容（类型、函数声明等） */

#endif /* PROJECT_SUBDIR_FILE_H */
```

- 生成器须自动根据目标头文件路径生成唯一且符合上述规则的宏名，避免宏冲突。
- 在 Doxygen 注释中仍保留 @file，并与 include guard 宏一致的注释说明（可选）。
- 如果项目已有统一的 include 根目录，agent 应基于该根路径生成宏前缀，确保不同模块间互不冲突。

//
