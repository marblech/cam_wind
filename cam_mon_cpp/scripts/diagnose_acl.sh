#!/usr/bin/env bash
set -euo pipefail

# diagnose_acl.sh - ACL 运行时诊断脚本
# 用法: REMOTE_HOST=user@host bash scripts/diagnose_acl.sh

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_success() { echo -e "${GREEN}[✓]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[⚠]${NC} $*"; }
log_error() { echo -e "${RED}[✗]${NC} $*"; }

REMOTE_HOST="${REMOTE_HOST:-root@192.168.8.103}"
REMOTE_DIR="${REMOTE_DIR:-/root/vdec_test}"

if [ -z "${REMOTE_HOST}" ]; then
  log_error "未设置 REMOTE_HOST 环境变量"
  log_info "用法: REMOTE_HOST=user@host bash scripts/diagnose_acl.sh"
  exit 1
fi

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║     ACL 运行时环境诊断工具                 ║"
echo "╚════════════════════════════════════════════╝"
echo ""
log_info "远程主机: ${REMOTE_HOST}"
log_info "项目目录: ${REMOTE_DIR}"
echo ""

# 创建远程诊断脚本
DIAG_SCRIPT=$(cat <<'DIAG_SCRIPT_END'
set -euo pipefail

echo "[1] 检查 ACL SDK 库文件..."
if [ -d "/usr/local/Ascend/ascend-toolkit/latest/runtime/lib64" ]; then
  ACL_LIBS=$(ls /usr/local/Ascend/ascend-toolkit/latest/runtime/lib64/libascendcl.so* 2>/dev/null | wc -l)
  DVPP_LIBS=$(ls /usr/local/Ascend/ascend-toolkit/latest/runtime/lib64/libacl_dvpp.so* 2>/dev/null | wc -l)
  
  if [ $ACL_LIBS -gt 0 ] && [ $DVPP_LIBS -gt 0 ]; then
    echo "✓ ACL SDK 库文件存在"
    ls -lh /usr/local/Ascend/ascend-toolkit/latest/runtime/lib64/libascendcl.so* 2>/dev/null | head -1
    ls -lh /usr/local/Ascend/ascend-toolkit/latest/runtime/lib64/libacl_dvpp.so* 2>/dev/null | head -1
  else
    echo "✗ 缺少 ACL SDK 库文件 (ACL=$ACL_LIBS, DVPP=$DVPP_LIBS)"
  fi
else
  echo "✗ ACL SDK 目录不存在: /usr/local/Ascend/ascend-toolkit/latest/runtime/lib64"
fi

echo ""
echo "[2] 检查可执行文件链接..."
BUILD_DIR="$REMOTE_DIR/build/bin/decode_video_example"
if [ -f "$BUILD_DIR" ]; then
  if ldd "$BUILD_DIR" | grep -q "libascendcl\|libacl_dvpp"; then
    echo "✓ 可执行文件已正确链接 ACL 库"
    ldd "$BUILD_DIR" | grep -E "libascendcl|libacl_dvpp|not found" || true
  else
    echo "✗ 可执行文件未链接 ACL 库"
    ldd "$BUILD_DIR" | grep -E "not found" && echo "缺失的库:" || true
  fi
else
  echo "⚠ 未找到可执行文件: $BUILD_DIR"
fi

echo ""
echo "[3] 检查硬件设备..."
if ls /dev/davinci* 2>/dev/null | head -3; then
  echo "✓ 找到硬件设备"
else
  echo "⚠ 未找到硬件设备 (/dev/davinci*)"
fi

echo ""
echo "[4] 检查当前环境变量..."
if [ -n "${LD_LIBRARY_PATH:-}" ]; then
  echo "LD_LIBRARY_PATH:"
  echo "  ${LD_LIBRARY_PATH}" | tr ':' '\n' | sed 's/^/  /'
else
  echo "⚠ LD_LIBRARY_PATH 未设置"
fi

if [ -n "${ASCEND_HOME:-}" ]; then
  echo "ASCEND_HOME: ${ASCEND_HOME}"
else
  echo "⚠ ASCEND_HOME 未设置"
fi

if [ -n "${ASCEND_RT_VISIBLE_DEVICES:-}" ]; then
  echo "ASCEND_RT_VISIBLE_DEVICES: ${ASCEND_RT_VISIBLE_DEVICES}"
else
  echo "⚠ ASCEND_RT_VISIBLE_DEVICES 未设置（ACL 可能无法访问设备）"
fi

echo ""
echo "[5] 测试运行可执行文件..."
if [ -f "$REMOTE_DIR/build/bin/decode_video_example" ]; then
  export LD_LIBRARY_PATH="/usr/local/Ascend/ascend-toolkit/latest/runtime/lib64:${LD_LIBRARY_PATH:-}"
  export ASCEND_HOME="/usr/local/Ascend/ascend-toolkit/latest"
  export ASCEND_RT_VISIBLE_DEVICES="${ASCEND_RT_VISIBLE_DEVICES:-0}"
  
  cd "$REMOTE_DIR/build"
  TEST_VIDEO="/tmp/test.mp4"
  
  if [ -f "$TEST_VIDEO" ]; then
    echo "✓ 测试视频存在: $TEST_VIDEO"
    if timeout 5 ./bin/decode_video_example "$TEST_VIDEO" 2>&1 | head -20; then
      echo "✓ 程序运行成功"
    else
      echo "✗ 程序运行失败（可能需要更多时间或不同的视频）"
    fi
  else
    echo "⚠ 测试视频不存在: $TEST_VIDEO"
    echo "   创建测试视频: ffmpeg -f lavfi -i color=c=black:s=320x240:d=1 $TEST_VIDEO"
  fi
else
  echo "✗ 未找到可执行文件"
fi
DIAG_SCRIPT_END
)

# 执行远程诊断
ssh "${REMOTE_HOST}" bash -lc "
  REMOTE_DIR='${REMOTE_DIR}'
  $DIAG_SCRIPT
" || {
  log_error "远程诊断执行失败"
  exit 1
}

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║          诊断完成                          ║"
echo "╚════════════════════════════════════════════╝"
echo ""

log_info "建议修复步骤:"
echo "  1. 确保 ACL SDK 已安装在远程主机"
echo "  2. 设置环境变量:"
echo "     export LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/latest/runtime/lib64:\$LD_LIBRARY_PATH"
echo "     export ASCEND_HOME=/usr/local/Ascend/ascend-toolkit/latest"
echo "  3. 在 ~/.bashrc 中添加上述命令以持久化配置"
echo "  4. 重新编译: bash scripts/remote_build.sh"
echo ""
