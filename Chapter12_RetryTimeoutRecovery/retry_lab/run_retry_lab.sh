#!/usr/bin/env bash
set -euo pipefail

BIN_DIR="${CH12_BIN_DIR:-$HOME/bin}"
BASE_PORT="${CH12_PORT:-9100}"
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

temp_dir="$(mktemp -d)"
server_pid=""
case_number=0

cleanup()
{
    if [[ -n "$server_pid" ]] && kill -0 "$server_pid" 2>/dev/null; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi

    rm -rf "$temp_dir"
}
trap cleanup EXIT INT TERM

run_case()
{
    local title="$1"
    local mode="$2"
    local expected_status="$3"
    shift 3

    case_number=$((case_number + 1))
    local port=$((BASE_PORT + case_number - 1))
    local server_log="$temp_dir/server_${case_number}.log"
    local client_log="$temp_dir/client_${case_number}.log"

    "$SERVER" --port "$port" --mode "$mode" >"$server_log" 2>&1 &
    server_pid=$!

    sleep 0.15

    if ! kill -0 "$server_pid" 2>/dev/null; then
        echo "Server exited before case '$title' could run:" >&2
        cat "$server_log" >&2
        exit 1
    fi

    set +e
    "$CLIENT" \
        --server 127.0.0.1 \
        --port "$port" \
        "$@" >"$client_log" 2>&1
    local client_status=$?
    set -e

    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    server_pid=""

    echo
    echo "============================================================"
    echo "$title"
    echo "============================================================"
    echo "--- client ---"
    cat "$client_log"
    echo
    echo "--- server ---"
    cat "$server_log"

    if [[ "$client_status" -ne "$expected_status" ]]; then
        echo >&2
        echo "Case '$title' returned $client_status; expected $expected_status." >&2
        exit 1
    fi
}

run_case \
    "1. Normal request/ACK" \
    normal \
    0 \
    --interval-ms 5000 \
    --transaction 42

run_case \
    "2. Lost ACK, timeout, retry, and duplicate detection" \
    drop-first-ack \
    0 \
    --interval-ms 5000 \
    --transaction 43

run_case \
    "3. Retryable NACK (busy once)" \
    busy-once \
    0 \
    --interval-ms 5000 \
    --transaction 44

run_case \
    "4. Non-retryable NACK (invalid value)" \
    normal \
    2 \
    --interval-ms 50 \
    --transaction 45

run_case \
    "5. Retry budget exhausted and recovery" \
    silent \
    2 \
    --interval-ms 5000 \
    --transaction 46

echo
echo "All Chapter 12 retry lab scenarios behaved as expected."
