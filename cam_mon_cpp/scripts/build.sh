#!/usr/bin/env bash
# ============================================================================
# 统一构建管理脚本 - 支持本地和远程编译
# 用法: ./scripts/build.sh [选项]
# ============================================================================

set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# 默认配置
TARGET="local"                              # local / remote
BUILD_TYPE="Release"                        # Release / Debug / RelWithDebInfo / MinSizeRel
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
RUN_CMD=""
SHOW_HELP=false
DO_CLEAN=false

# 日志函数
log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_success() { echo -e "${GREEN}[✓]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[⚠]${NC} $*"; }
log_error() { echo -e "${RED}[✗]${NC} $*"; }

# 显示帮助
show_help() {
    cat << 'EOF'
📚 统一构建管理脚本

用法:
  ./scripts/build.sh [选项]

选项:
  -l, --local               本地编译（默认）
  -r, --remote              远程编译
  -d, --debug               Debug构建类型
  -R, --release             Release构建类型（默认）
  -j, --jobs N              并行编译线程数（默认：CPU核心数）
  -x, --execute CMD         编译后执行命令
  -h, --help                显示此帮助

环境变量:
  REMOTE_HOST               远程主机（user@host）
  REMOTE_DIR                远程项目目录
  BUILD_TYPE                构建类型
  JOBS                      并行线程数

示例:
  # 本地Release编译
  ./scripts/build.sh
  
  # 本地Debug编译
  ./scripts/build.sh --debug
  
  # 远程Release编译
  REMOTE_HOST=user@host ./scripts/build.sh --remote
  
  # 远程编译并运行
  REMOTE_HOST=user@host ./scripts/build.sh --remote --execute "./bin/decode_video_example /tmp/test.mp4"
  
  # 使用更多线程
  ./scripts/build.sh --jobs 8

EOF
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -l|--local)
            TARGET="local"
            shift
            ;;
        -r|--remote)
            TARGET="remote"
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -R|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -x|--execute)
            RUN_CMD="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        --clean)
            DO_CLEAN=true
            shift
            ;;
        *)
            log_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
done

# 本地编译
build_local() {
    local BUILD_DIR="${PROJECT_ROOT}/build"
    
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║       本地构建 - 编译配置                  ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    log_info "项目目录: ${PROJECT_ROOT}"
    log_info "编译目录: ${BUILD_DIR}"
    log_info "构建类型: ${BUILD_TYPE}"
    log_info "并行线程: ${JOBS}"
    [ -n "${RUN_CMD}" ] && log_info "执行命令: ${RUN_CMD}"
    echo ""
    
    # 创建编译目录
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    # CMake配置
    log_info "CMake配置..."
    if ! cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"; then
        log_error "CMake配置失败"
        return 1
    fi
    log_success "CMake配置完成"
    echo ""
    
    # 编译
    log_info "编译源代码（使用${JOBS}个线程）..."
    if ! cmake --build "${BUILD_DIR}" --target all -- -j${JOBS}; then
        log_error "编译失败"
        return 1
    fi
    log_success "编译完成"
    echo ""
    
    # 运行（如果指定）
    if [ -n "${RUN_CMD}" ]; then
        log_info "执行: ${RUN_CMD}"
        echo "─────────────────────────────────────────"
        cd "${BUILD_DIR}"
        if ! eval "${RUN_CMD}"; then
            log_error "执行失败"
            return 1
        fi
        echo "─────────────────────────────────────────"
        log_success "执行完成"
        echo ""
    fi
    
    return 0
}

# 本地清理
clean_local() {
    local BUILD_DIR="${PROJECT_ROOT}/build"

    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║       本地清理 - 删除构建产物              ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    log_info "清理目录: ${BUILD_DIR}"
    if [ -d "${BUILD_DIR}" ]; then
        rm -rf "${BUILD_DIR}"
        log_success "已删除 ${BUILD_DIR}"
    else
        log_info "没有找到构建目录 ${BUILD_DIR}，无需清理"
    fi
    return 0
}

# 远程编译
build_remote() {
    local REMOTE_HOST="${REMOTE_HOST:-}"
    local REMOTE_DIR="${REMOTE_DIR:-~/projects/VideoDecodeLib}"
    
    if [ -z "${REMOTE_HOST}" ]; then
        log_error "未设置REMOTE_HOST环境变量"
        log_info "请设置: export REMOTE_HOST='user@host'"
        return 1
    fi
    
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║       远程构建 - 编译配置                  ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    log_info "项目目录: ${PROJECT_ROOT}"
    log_info "远程主机: ${REMOTE_HOST}"
    log_info "远程目录: ${REMOTE_DIR}"
    log_info "构建类型: ${BUILD_TYPE}"
    log_info "并行线程: ${JOBS}"
    [ -n "${RUN_CMD}" ] && log_info "执行命令: ${RUN_CMD}"
    echo ""
    
    # 调用远程编译脚本
    if [ "${DO_CLEAN}" = true ] || [ "${CLEAN_REMOTE:-0}" = "1" ]; then
        BUILD_TYPE="${BUILD_TYPE}" JOBS="${JOBS}" RUN_CMD="${RUN_CMD}" \
            REMOTE_HOST="${REMOTE_HOST}" REMOTE_DIR="${REMOTE_DIR}" \
            bash "${SCRIPT_DIR}/remote_clean.sh"
    else
        BUILD_TYPE="${BUILD_TYPE}" JOBS="${JOBS}" RUN_CMD="${RUN_CMD}" \
            REMOTE_HOST="${REMOTE_HOST}" REMOTE_DIR="${REMOTE_DIR}" \
            bash "${SCRIPT_DIR}/remote_build.sh"
    fi
    
    return $?
}

# 主程序
main() {
    echo "╔════════════════════════════════════════════╗"
    echo "║     🔨 VideoDecodeLib 统一构建系统         ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    
    case "${TARGET}" in
        local)
            log_info "开始本地编译..."
            if build_local; then
                echo ""
                echo "╔════════════════════════════════════════════╗"
                echo "║            编译结果：成功 ✓                 ║"
                echo "╚════════════════════════════════════════════╝"
                echo ""
                log_success "本地编译成功完成！"
                echo "  📂 输出目录: ${PROJECT_ROOT}/build"
                echo "  📄 可执行文件: ${PROJECT_ROOT}/build/bin/"
                echo "  📚 库文件: ${PROJECT_ROOT}/build/lib/"
                echo ""
                return 0
            else
                echo ""
                log_error "本地编译失败"
                return 1
            fi
            ;;
        remote)
            log_info "开始远程编译..."
            if build_remote; then
                echo ""
                echo "╔════════════════════════════════════════════╗"
                echo "║            编译结果：成功 ✓                 ║"
                echo "╚════════════════════════════════════════════╝"
                echo ""
                log_success "远程编译成功完成！"
                return 0
            else
                echo ""
                log_error "远程编译失败"
                return 1
            fi
            ;;
        *)
            log_error "未知的构建目标: ${TARGET}"
            return 1
            ;;
    esac
}

# 执行主程序
main
exit $?
