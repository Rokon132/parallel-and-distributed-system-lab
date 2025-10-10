#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
SERVER_BIN="${PROJECT_ROOT}/matrixOp_server"
CLIENT_BIN="${PROJECT_ROOT}/matrixOp_client"

if [[ ! -x "${SERVER_BIN}" || ! -x "${CLIENT_BIN}" ]]; then
  echo "Please build the server and client binaries before running this script." >&2
  exit 1
fi

if ! pgrep -x rpcbind >/dev/null 2>&1; then
  echo "rpcbind service is not running. Start it with 'sudo systemctl start rpcbind' (or equivalent) first." >&2
  exit 1
fi

TEMP_OUTPUT=$(mktemp)
SERVER_LOG=$(mktemp)

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]]; then
    kill "${SERVER_PID}" >/dev/null 2>&1 || true
  fi
  rm -f "${TEMP_OUTPUT}" "${SERVER_LOG}"
}
trap cleanup EXIT

"${SERVER_BIN}" >"${SERVER_LOG}" 2>&1 &
SERVER_PID=$!

sleep 1

cat <<'EOF' | "${CLIENT_BIN}" localhost | tee "${TEMP_OUTPUT}"
1
2 2
1 2 3 4
2 2
5 6 7 8
3
2 3
1 2 3 4 5 6
4
2 2
4 7 2 6
0
EOF

echo "Client interaction transcript saved to ${TEMP_OUTPUT}"
