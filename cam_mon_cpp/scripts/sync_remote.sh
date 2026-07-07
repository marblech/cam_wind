#!/usr/bin/env bash
# ============================================================================
# 仅同步源代码到远程，不进行编译
# 用途：快速同步代码更改到远程服务器
# ============================================================================

set -euo pipefail

# 配置项
REMOTE_HOST="${REMOTE_HOST:-user@remote-host}"
REMOTE_DIR="${REMOTE_DIR:-~/projects/VideoDecodeLib}"

# 本地项目根目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "╔════════════════════════════════════════════╗"
echo "║      远程代码同步（仅同步，不编译）      ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "📁 本地项目: ${PROJECT_ROOT}"
echo "🖥️  远程主机: ${REMOTE_HOST}"
echo "📂 远程目录: ${REMOTE_DIR}"
echo ""

# 同步项目到远程（排除构建产物和临时文件）
echo "📤 开始同步代码..."
rsync -avz --delete \
  --exclude 'build/' \
  --exclude '.git/' \
  --exclude '.cache/' \
  --exclude '.vscode/' \
  --exclude '*.o' \
  --exclude '*.a' \
  --exclude '*.so' \
  "${PROJECT_ROOT}/" "${REMOTE_HOST}:${REMOTE_DIR}/"

echo ""
echo "✅ 代码同步完成！"
echo ""
echo "下一步可执行："
echo "  ssh ${REMOTE_HOST}"
echo "  cd ${REMOTE_DIR}"
echo "  ./build.sh"
