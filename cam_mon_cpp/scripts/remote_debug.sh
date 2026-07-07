#!/usr/bin/env bash
# ============================================================================
# 启动远程GDB调试服务器
# 用法: ./remote_debug.sh [程序名] [参数...]
# 示例: ./remote_debug.sh decode_video_example /path/to/video.mp4
# ============================================================================

set -euo pipefail

# 配置项
REMOTE_HOST="${REMOTE_HOST:-user@remote-host}"
REMOTE_DIR="${REMOTE_DIR:-~/projects/VideoDecodeLib}"
GDB_PORT="${GDB_PORT:-5039}"
PROGRAM_NAME="${1:-decode_video_example}"
shift || true
PROGRAM_ARGS=("$@")

PROGRAM_PATH="${REMOTE_DIR}/build/bin/${PROGRAM_NAME}"

echo "╔════════════════════════════════════════════╗"
echo "║       远程GDB调试服务器                   ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "🖥️  远程主机: ${REMOTE_HOST}"
echo "📂 程序路径: ${PROGRAM_PATH}"
echo "🔌 GDB端口: ${GDB_PORT}"
echo "📝 参数: ${PROGRAM_ARGS[*]:-无}"
echo ""
echo "💡 提示: 在本地VS Code中配置远程调试，连接到 ${REMOTE_HOST}:${GDB_PORT}"
echo ""
echo "▶️  启动GDB服务器..."
echo "─────────────────────────────────────────────"

ssh "${REMOTE_HOST}" bash -lc "
  export LD_LIBRARY_PATH='${REMOTE_DIR}/build/lib:\$LD_LIBRARY_PATH'
  gdbserver 0.0.0.0:${GDB_PORT} '${PROGRAM_PATH}' ${PROGRAM_ARGS[*]:-}
"
