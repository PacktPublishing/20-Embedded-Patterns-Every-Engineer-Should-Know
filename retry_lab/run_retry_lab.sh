#!/usr/bin/env bash
set -euo pipefail

BIN_DIR="${CH12_BIN_DIR:-$HOME/bin}"
PORT="${CH12_PORT:-9100}"
SERVER="$BIN_DIR/ch12_retry_server"
CLIENT="$BIN_DIR/ch12_retry_client"

if [[ ! -x "$SERVER" || ! -x "$CLIENT" ]]; then
    echo "Chapter 12 executables were not found in $BIN_DIR." >&2
    echo "Build and install the lab first:" >&2
    echo "  cmake -S . -B build" >&2
    echo "  cmake --build build" >&2
    echo "  cmake --install build" >&2
    exit 1
fi

server_log="$(mktemp)"
server_pid=""

cleanup()
{
    if [[ -n "$server_pid" ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
    rm -f "$server_log"
}
trap cleanup EXIT INT TERM

"$SERVER" --port "$PORT" >"$server_log" 2>&1 &
server_pid=$!

# Give the server a moment to bind its UDP socket before sending the request.
sleep 0.2

if ! kill -0 "$server_pid" 2>/dev/null; then
    echo "Server exited before the client ran:" >&2
    cat "$server_log" >&2
    exit 1
fi

echo "--- client ---"
"$CLIENT" \
    --server 127.0.0.1 \
    --port "$PORT" \
    --interval-ms 5000 \
    --transaction 42

# Allow the server's final output to be flushed before displaying it.
sleep 0.1

echo
echo "--- server ---"
cat "$server_log"
