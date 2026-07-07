#!/usr/bin/env bash
# ============================================================================
# 远程编译环境检查脚本
# 检查本地和远程环境是否满足编译要求
# ============================================================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 日志函数
log_info() { echo -e "${BLUE}ℹ${NC} $*"; }
log_pass() { echo -e "${GREEN}✓${NC} $*"; }
log_fail() { echo -e "${RED}✗${NC} $*"; }
log_warn() { echo -e "${YELLOW}⚠${NC} $*"; }

# 计数器
CHECKS_PASSED=0
CHECKS_FAILED=0
CHECKS_WARN=0

# 检查本地工具
check_local_tools() {
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║       本地环境检查                        ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    
    # SSH
    if command -v ssh &> /dev/null; then
        log_pass "SSH 已安装: $(ssh -V 2>&1 | head -1)"
        ((CHECKS_PASSED++))
    else
        log_fail "SSH 未安装"
        ((CHECKS_FAILED++))
    fi
    
    # rsync
    if command -v rsync &> /dev/null; then
        log_pass "rsync 已安装: $(rsync --version 2>&1 | head -1)"
        ((CHECKS_PASSED++))
    else
        log_fail "rsync 未安装 (需要用于代码同步)"
        ((CHECKS_FAILED++))
    fi
    
    # CMake
    if command -v cmake &> /dev/null; then
        log_pass "CMake 已安装: $(cmake --version | head -1)"
        ((CHECKS_PASSED++))
    else
        log_warn "CMake 未安装 (本地编译需要)"
        ((CHECKS_WARN++))
    fi
    
    # GCC/Clang
    if command -v gcc &> /dev/null; then
        log_pass "GCC 已安装: $(gcc --version | head -1)"
        ((CHECKS_PASSED++))
    elif command -v clang &> /dev/null; then
        log_pass "Clang 已安装: $(clang --version | head -1)"
        ((CHECKS_PASSED++))
    else
        log_warn "C++ 编译器未安装 (本地编译需要)"
        ((CHECKS_WARN++))
    fi
    
    # GDB
    if command -v gdb &> /dev/null; then
        log_pass "GDB 已安装: $(gdb --version | head -1)"
        ((CHECKS_PASSED++))
    else
        log_warn "GDB 未安装 (调试需要)"
        ((CHECKS_WARN++))
    fi
}

# 检查SSH连接
check_ssh_connection() {
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║       SSH 连接检查                        ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    
    local REMOTE_HOST="${REMOTE_HOST:-}"
    
    if [ -z "${REMOTE_HOST}" ]; then
        log_warn "REMOTE_HOST 未设置"
        echo "    设置: export REMOTE_HOST='user@host'"
        ((CHECKS_WARN++))
        return
    fi
    
    log_info "检查SSH连接到: ${REMOTE_HOST}"
    
    if ssh -o ConnectTimeout=5 -o BatchMode=yes "${REMOTE_HOST}" "echo 'SSH OK'" &> /dev/null; then
        log_pass "SSH 连接成功"
        ((CHECKS_PASSED++))
    else
        log_fail "SSH 连接失败"
        echo "    检查:"
        echo "    1. 主机地址是否正确: ${REMOTE_HOST}"
        echo "    2. SSH密钥是否已配置: ssh-copy-id -i ~/.ssh/id_rsa.pub ${REMOTE_HOST}"
        echo "    3. 网络连接是否正常"
        ((CHECKS_FAILED++))
        return
    fi
    
    # SSH密钥认证
    if ssh-keygen -F "${REMOTE_HOST}" &> /dev/null 2>&1; then
        log_pass "SSH密钥认证已配置"
        ((CHECKS_PASSED++))
    else
        log_warn "SSH密钥认证未配置（可能需要每次输入密码）"
        echo "    配置: ssh-copy-id -i ~/.ssh/id_rsa.pub ${REMOTE_HOST}"
        ((CHECKS_WARN++))
    fi
}

# 检查远程环境
check_remote_environment() {
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║       远程环境检查                        ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    
    local REMOTE_HOST="${REMOTE_HOST:-}"
    local REMOTE_DIR="${REMOTE_DIR:-}"
    
    if [ -z "${REMOTE_HOST}" ]; then
        log_warn "REMOTE_HOST 未设置，跳过远程检查"
        return
    fi
    
    log_info "连接到: ${REMOTE_HOST}"
    
    # CMake
    log_info "检查 CMake..."
    if ssh "${REMOTE_HOST}" "command -v cmake" &> /dev/null; then
        local CMAKE_VERSION=$(ssh "${REMOTE_HOST}" "cmake --version" 2>/dev/null | head -1)
        log_pass "CMake 已安装: ${CMAKE_VERSION}"
        ((CHECKS_PASSED++))
    else
        log_fail "CMake 未安装"
        ((CHECKS_FAILED++))
    fi
    
    # GCC
    log_info "检查 GCC..."
    if ssh "${REMOTE_HOST}" "command -v gcc" &> /dev/null; then
        local GCC_VERSION=$(ssh "${REMOTE_HOST}" "gcc --version" 2>/dev/null | head -1)
        log_pass "GCC 已安装: ${GCC_VERSION}"
        ((CHECKS_PASSED++))
    else
        log_fail "GCC 未安装"
        ((CHECKS_FAILED++))
    fi
    
    # FFMPEG
    log_info "检查 FFMPEG..."
    if ssh "${REMOTE_HOST}" "pkg-config --list-all | grep -q ffmpeg" &> /dev/null; then
        local FFMPEG_VERSION=$(ssh "${REMOTE_HOST}" "pkg-config --modversion libavformat" 2>/dev/null)
        log_pass "FFMPEG 已安装: libavformat ${FFMPEG_VERSION}"
        ((CHECKS_PASSED++))
    else
        log_warn "FFMPEG 未安装或pkg-config未找到"
        ((CHECKS_WARN++))
    fi
    
    # ACL SDK
    log_info "检查 ACL SDK..."
    if ssh "${REMOTE_HOST}" "test -f /usr/local/Ascend/acllib/include/acl/acl.h" &> /dev/null; then
        log_pass "ACL SDK 已安装: /usr/local/Ascend/acllib"
        ((CHECKS_PASSED++))
    else
        log_warn "ACL SDK 未在 /usr/local/Ascend/acllib"
        log_info "  检查其他位置: ssh ${REMOTE_HOST} 'find / -name acl.h 2>/dev/null'"
        ((CHECKS_WARN++))
    fi
    
    # GDB
    log_info "检查 GDB..."
    if ssh "${REMOTE_HOST}" "command -v gdb" &> /dev/null; then
        local GDB_VERSION=$(ssh "${REMOTE_HOST}" "gdb --version" 2>/dev/null | head -1)
        log_pass "GDB 已安装: ${GDB_VERSION}"
        ((CHECKS_PASSED++))
    else
        log_warn "GDB 未安装（远程调试需要）"
        ((CHECKS_WARN++))
    fi
    
    # 远程目录
    if [ -n "${REMOTE_DIR}" ]; then
        log_info "检查远程项目目录..."
        if ssh "${REMOTE_HOST}" "test -d ${REMOTE_DIR}" &> /dev/null; then
            log_pass "远程目录存在: ${REMOTE_DIR}"
            ((CHECKS_PASSED++))
        else
            log_warn "远程目录不存在: ${REMOTE_DIR}"
            echo "    创建: ssh ${REMOTE_HOST} mkdir -p ${REMOTE_DIR}"
            ((CHECKS_WARN++))
        fi
    fi
}

# 检查VS Code配置
check_vscode_config() {
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║       VS Code 配置检查                     ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    
    local PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    
    # tasks.json
    if [ -f "${PROJECT_ROOT}/.vscode/tasks.json" ]; then
        log_pass "tasks.json 已配置"
        ((CHECKS_PASSED++))
    else
        log_warn "tasks.json 未找到"
        ((CHECKS_WARN++))
    fi
    
    # launch.json
    if [ -f "${PROJECT_ROOT}/.vscode/launch.json" ]; then
        log_pass "launch.json 已配置"
        ((CHECKS_PASSED++))
    else
        log_warn "launch.json 未找到"
        ((CHECKS_WARN++))
    fi
    
    # settings.json
    if [ -f "${PROJECT_ROOT}/.vscode/settings.json" ]; then
        log_pass "settings.json 已配置"
        ((CHECKS_PASSED++))
    else
        log_warn "settings.json 未找到"
        ((CHECKS_WARN++))
    fi
}

# 建议
show_recommendations() {
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║            检查总结                       ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    echo -e "${GREEN}通过${NC}: ${CHECKS_PASSED}"
    echo -e "${YELLOW}警告${NC}: ${CHECKS_WARN}"
    echo -e "${RED}失败${NC}: ${CHECKS_FAILED}"
    echo ""
    
    if [ ${CHECKS_FAILED} -gt 0 ]; then
        log_fail "环境检查失败，请修复上述问题"
        echo ""
        echo "常见修复方法:"
        echo ""
        echo "1. 安装 rsync"
        echo "   sudo apt-get install rsync  # Ubuntu/Debian"
        echo "   sudo yum install rsync      # CentOS/RHEL"
        echo ""
        echo "2. 配置 SSH 密钥"
        echo "   ssh-keygen -t rsa -b 4096 -f ~/.ssh/id_rsa -N ''"
        echo "   ssh-copy-id -i ~/.ssh/id_rsa.pub user@host"
        echo ""
        echo "3. 在远程安装依赖"
        echo "   ssh user@host 'sudo apt-get install cmake gcc build-essential'"
        echo "   ssh user@host 'sudo apt-get install libavformat-dev libavcodec-dev'"
        echo ""
        return 1
    elif [ ${CHECKS_WARN} -gt 0 ]; then
        log_warn "环境检查有警告，建议处理"
        echo ""
        echo "可选的改进:"
        echo "1. 配置 SSH 密钥认证（避免每次输入密码）"
        echo "2. 在远程安装 GDB（用于远程调试）"
        echo "3. 确保 ACL SDK 正确安装"
        echo ""
        return 0
    else
        log_pass "环境检查全部通过！"
        echo ""
        echo "您现在可以:"
        echo "1. 在 VS Code 中使用任务进行编译: Ctrl+Shift+B"
        echo "2. 使用远程编译: bash scripts/remote_build.sh"
        echo "3. 启动远程调试: bash scripts/remote_debug.sh"
        echo ""
        return 0
    fi
}

# 主程序
main() {
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║    📋 远程编译环境检查工具                 ║"
    echo "╚════════════════════════════════════════════╝"
    
    check_local_tools
    check_ssh_connection
    check_remote_environment
    check_vscode_config
    show_recommendations
    
    return $?
}

main
exit $?
