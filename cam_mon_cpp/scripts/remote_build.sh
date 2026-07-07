#!/usr/bin/env bash
set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 配置项（按需修改/或通过环境变量覆盖）
REMOTE_HOST="root@192.168.8.103"          # 远程主机（格式：user@host 或 host）
REMOTE_DIR="/root/cam_mon_cpp"   # 远程目录（默认改为 /root/cam_mon_cpp）
BUILD_TYPE="Debug"                     # 构建类型：Release/Debug/RelWithDebInfo/MinSizeRel
JOBS="4" # 并行编译线程数
RUN_CMD="${RUN_CMD:-}"                                  # 远端运行命令（可选），例如：./bin/decode_video_example /tmp/test.mp4
REMOTE_ENV="${REMOTE_ENV:-}"                            # 远端运行所需环境变量（可选）

# 本地项目根目录（脚本所在目录的上一级为项目根）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[✓]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[⚠]${NC} $*"
}

log_error() {
    echo -e "${RED}[✗]${NC} $*"
}

# 打印配置信息
echo ""
echo "╔════════════════════════════════════════════╗"
echo "║       远程编译脚本 - 构建配置信息        ║"
echo "╚════════════════════════════════════════════╝"
echo ""
log_info "项目根目录: ${PROJECT_ROOT}"
log_info "远程主机:   ${REMOTE_HOST}"
log_info "远程目录:   ${REMOTE_DIR}"
log_info "构建类型:   ${BUILD_TYPE}"
log_info "并行线程:   ${JOBS}"
[ -n "${RUN_CMD}" ] && log_info "运行命令:   ${RUN_CMD}"
echo ""

# 步骤1: 验证SSH连接
log_info "验证SSH连接..."
if ! ssh -o ConnectTimeout=5 "${REMOTE_HOST}" "echo 'SSH连接正常'" > /dev/null 2>&1; then
    log_error "无法连接到远程主机 ${REMOTE_HOST}"
    log_info "请检查："
    log_info "  1. SSH主机地址是否正确"
    log_info "  2. 网络连接是否正常"
    log_info "  3. SSH密钥认证是否已配置"
    log_info "配置: export REMOTE_HOST='user@host'"
    exit 1
fi
log_success "SSH连接正常"
echo ""

# 步骤2: 同步项目到远程（排除本地构建产物和临时文件）
# 在同步之前，确保远程目录存在（避免 rsync 因目录不存在失败）
log_info "确保远程目录存在..."
if ! ssh "${REMOTE_HOST}" "mkdir -p '${REMOTE_DIR}'" >/dev/null 2>&1; then
    log_error "无法在远程创建目录 ${REMOTE_DIR}"
    log_info "请检查远程主机权限或路径是否正确"
    exit 1
fi
log_success "远程目录准备就绪"

log_info "开始同步代码到远程..."
if rsync -az --delete \
  --exclude 'build/' \
  --exclude '.git/' \
  --exclude '.cache/' \
  --exclude '.vscode/' \
  --exclude '*.o' \
  --exclude '*.a' \
  --exclude '*.so' \
  "${PROJECT_ROOT}/" "${REMOTE_HOST}:${REMOTE_DIR}/" 2>&1; then
    log_success "代码同步完成"
else
    log_error "代码同步失败"
    log_info "请检查rsync是否已安装: which rsync"
    exit 1
fi
echo ""

# 步骤3: 在远程执行构建
log_info "开始远程编译..."
log_info "  - CMake配置..."
log_info "  - 编译源代码（使用${JOBS}个线程）..."
log_info "  - 链接库文件..."
echo ""

BUILD_START=$(date +%s)

# 构建远程执行脚本
REMOTE_SCRIPT=$(cat <<'REMOTE_SCRIPT_END'
set -euo pipefail

# 修复 ACL 库版本链接（如果缺失）
ACL_LIB_DIR="/usr/local/Ascend/ascend-toolkit/latest/runtime/lib64"
if [ -d "$ACL_LIB_DIR" ] && [ ! -L "$ACL_LIB_DIR/libascendcl.so.1" ] && [ ! -f "$ACL_LIB_DIR/libascendcl.so.1" ]; then
  ln -sf libascendcl.so "$ACL_LIB_DIR/libascendcl.so.1" 2>/dev/null || true
fi

# 设置 ACL SDK 环境变量（Ascend 硬件加速）
export LD_LIBRARY_PATH="/usr/local/Ascend/ascend-toolkit/latest/runtime/lib64:${LD_LIBRARY_PATH:-}"
export ASCEND_HOME="/usr/local/Ascend/ascend-toolkit/latest"
export ASCEND_RT_VISIBLE_DEVICES="${ASCEND_RT_VISIBLE_DEVICES:-0}"

# 如果传入 REMOTE_ENV（例如 BOOST_ROOT=/root/boost_182），则在远程提前导出，确保 CMake 能找到依赖
if [ -n "$REMOTE_ENV" ]; then
  eval "export $REMOTE_ENV"
fi

# 远程日志函数
log_info() { echo '[INFO]' "$@"; }
log_success() { echo '✓' "$@"; }
log_error() { echo '✗' "$@"; }

# 删除旧的编译目录
log_info '清理旧的编译目录...'
rm -rf "$REMOTE_DIR/build"

# 创建编译目录
mkdir -p "$REMOTE_DIR/build"

# CMake配置
log_info '执行CMake配置...'
cmake -S "$REMOTE_DIR" -B "$REMOTE_DIR/build" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_MESSAGE_LOG_LEVEL=STATUS || {
    log_error 'CMake配置失败'
    exit 1
  }

# 编译
log_info '编译源代码...'
cmake --build "$REMOTE_DIR/build" --target all -- -j"$JOBS" || {
  log_error '编译失败'
  exit 1
}

# 显式构建共享库（避免部分工程生成器/默认目标未包含该库）
log_info '编译共享库 video_decode_lib...'
cmake --build "$REMOTE_DIR/build" --target video_decode_lib -- -j"$JOBS" || {
  log_error 'video_decode_lib 编译失败'
  exit 1
}

log_success '编译完成'

log_info '产物检查（out 目录）:'
ls -la "$REMOTE_DIR/out" || true

# 如果配置了运行命令，则执行
if [ -n "$RUN_CMD" ]; then
  echo ''
  log_info "开始执行: $RUN_CMD"
  echo '─────────────────────────────────────────'
  cd "$REMOTE_DIR/build"
  if [ -n "$REMOTE_ENV" ]; then
    eval "export $REMOTE_ENV"
  fi
  eval "$RUN_CMD" || {
    log_error '执行失败'
    exit 1
  }
  echo '─────────────────────────────────────────'
  log_success '执行完成'
fi
REMOTE_SCRIPT_END
)

if ! ssh "${REMOTE_HOST}" bash -lc "
  REMOTE_DIR='${REMOTE_DIR}'
  BUILD_TYPE='${BUILD_TYPE}'
  JOBS='${JOBS}'
  RUN_CMD='${RUN_CMD}'
  REMOTE_ENV='${REMOTE_ENV}'
  
  $REMOTE_SCRIPT
"; then
    log_error "远程编译失败"
    exit 1
fi

BUILD_END=$(date +%s)
BUILD_TIME=$((BUILD_END - BUILD_START))

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║            编译结果总结                   ║"
echo "╚════════════════════════════════════════════╝"
echo ""
log_success "远程编译成功完成！"
log_info "耗时: ${BUILD_TIME} 秒"
log_info "输出位置: ${REMOTE_HOST}:${REMOTE_DIR}/build"
echo ""

if [ -z "${RUN_CMD}" ]; then
    log_info "提示: 未配置RUN_CMD，若需在远程执行程序，可使用:"
    log_info "  bash scripts/remote_run.sh decode_video_example /path/to/video.mp4"
fi

echo ""