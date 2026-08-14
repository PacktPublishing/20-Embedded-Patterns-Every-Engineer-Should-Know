#!/usr/bin/env bash

set -euo pipefail

SERVER="${HOME}/bin/ch12_retry_server"
CLIENT="${HOME}/bin/ch12_retry_client"

PORT=9100

server_log="$(mktemp)"
server_pid=""

cleanup()
{
    if [[ -n "${server_pid}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
        kill "${server_pid}" 2>/dev/null || true
        wait "${server_pid}" 2>/dev/null || true
    fi

    rm -f "${server_log}"
}

trap cleanup EXIT

"${SERVER}" --port "${PORT}" >"${server_log}" 2>&1 &
server_pid=$!

# Give the server a moment to bind its UDP socket.
sleep 0.2

echo "--- client ---"

"${CLIENT}" \
    --server 127.0.0.1 \
    --port "${PORT}" \
    --interval-ms 5000 \
    --transaction 42

# Stop the server and wait for it to exit so redirected stdout is flushed.
kill "${server_pid}"
wait "${server_pid}" || true
server_pid=""

echo
echo "--- server ---"
cat "${server_log}"
