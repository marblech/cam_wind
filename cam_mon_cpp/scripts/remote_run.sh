#!/usr/bin/env bash
# ============================================================================
# 在远程执行已编译的程序
# 用法: ./remote_run.sh [程序名] [参数...]
# 示例: ./remote_run.sh decode_video_example /path/to/video.mp4
# ============================================================================

set -euo pipefail

# 配置项
REMOTE_HOST="${REMOTE_HOST:-user@remote-host}"
REMOTE_DIR="${REMOTE_DIR:-~/projects/VideoDecodeLib}"
PROGRAM_NAME="${1:-decode_video_example}"
shift || true
PROGRAM_ARGS=("$@")

# 验证参数
if [ -z "$PROGRAM_NAME" ]; then
    echo "❌ 错误: 未指定程序名"
    echo "用法: $0 <程序名> [参数...]"
    exit 1
fi

PROGRAM_PATH="${REMOTE_DIR}/build/bin/${PROGRAM_NAME}"

echo "╔════════════════════════════════════════════╗"
echo "║       远程程序执行                        ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "🖥️  远程主机: ${REMOTE_HOST}"
echo "📂 程序路径: ${PROGRAM_PATH}"
echo "📝 参数: ${PROGRAM_ARGS[*]:-无}"
echo ""

# 执行远程程序
echo "▶️  开始执行..."
echo "─────────────────────────────────────────────"

ssh "${REMOTE_HOST}" bash -lc "
  export LD_LIBRARY_PATH='${REMOTE_DIR}/build/lib:\$LD_LIBRARY_PATH'
  '${PROGRAM_PATH}' ${PROGRAM_ARGS[*]:-}
"

echo ""
echo "✅ 执行完成"
