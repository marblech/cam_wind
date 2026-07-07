#!/usr/bin/env bash
set -euo pipefail

# remote_clean.sh
# 用于远程清理项目的 build 目录和临时文件。
# 环境变量:
#   REMOTE_HOST (必需) 例如 user@host
#   REMOTE_DIR  (可选, 默认: ~/projects/VideoDecodeLib)
# 参数:
#   --dry-run  仅打印将要删除的内容

DRY_RUN=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --dry-run)
      DRY_RUN=true
      shift
      ;;
    *)
      shift
      ;;
  esac
done

REMOTE_HOST="${REMOTE_HOST:-}"
REMOTE_DIR="${REMOTE_DIR:-~/projects/VideoDecodeLib}"

if [ -z "${REMOTE_HOST}" ]; then
  echo "[✗] 未设置 REMOTE_HOST 环境变量。使用: export REMOTE_HOST='user@host'"
  exit 1
fi

log_info() { echo -e "\033[0;34m[INFO]\033[0m $*"; }
log_success() { echo -e "\033[0;32m[✓]\033[0m $*"; }
log_warn() { echo -e "\033[1;33m[⚠]\033[0m $*"; }
log_error() { echo -e "\033[0;31m[✗]\033[0m $*"; }

log_info "准备在远程主机 ${REMOTE_HOST} 清理目录: ${REMOTE_DIR}/build"

if [ "${DRY_RUN}" = true ]; then
  log_warn "DRY RUN 模式 - 不会实际删除任何文件"
  ssh "${REMOTE_HOST}" "bash -lc 'echo "将删除: ${REMOTE_DIR}/build" && ls -ld ${REMOTE_DIR}/build || true'"
  exit 0
fi

# 执行远程删除
if ssh "${REMOTE_HOST}" "bash -lc 'if [ -d \"${REMOTE_DIR}/build\" ]; then rm -rf \"${REMOTE_DIR}/build\" && echo OK; else echo NO_DIR; fi'" | grep -q OK; then
  log_success "远程构建目录已删除: ${REMOTE_DIR}/build"
else
  log_info "远程构建目录不存在，或已被删除: ${REMOTE_DIR}/build"
fi

# 可选清理一些缓存/临时文件
if ssh "${REMOTE_HOST}" "bash -lc 'test -d \"${REMOTE_DIR}/.cache\" && rm -rf \"${REMOTE_DIR}/.cache\" && echo CACHE_OK || true'" | grep -q CACHE_OK; then
  log_success "远程 .cache 已删除"
fi

log_success "远程清理完成"
