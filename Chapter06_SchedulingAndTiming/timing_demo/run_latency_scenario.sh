#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

set -euo pipefail

MODE="quiet"
PERIOD_MS=10
SAMPLES=2000
WORK_US=500
PROBE_CPU=1
STRESS_CPU=1
STRESS_TIMEOUT="25s"
STRESS_WORKERS=1
OUT_DIR="results/ch06"

usage() {
    cat <<EOF
Usage:
  $0 [options]

Options:
  --mode quiet|same-core|isolated
  --period-ms N
  --samples N
  --work-us N
  --probe-cpu N
  --stress-cpu N
  --stress-timeout DURATION
  --stress-workers N
  --out-dir DIR
  --help

Examples:
  $0 --mode quiet
  $0 --mode same-core --probe-cpu 1 --stress-cpu 1
  $0 --mode isolated --probe-cpu 1 --stress-cpu 2
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode) MODE="$2"; shift 2 ;;
        --period-ms) PERIOD_MS="$2"; shift 2 ;;
        --samples) SAMPLES="$2"; shift 2 ;;
        --work-us) WORK_US="$2"; shift 2 ;;
        --probe-cpu) PROBE_CPU="$2"; shift 2 ;;
        --stress-cpu) STRESS_CPU="$2"; shift 2 ;;
        --stress-timeout) STRESS_TIMEOUT="$2"; shift 2 ;;
        --stress-workers) STRESS_WORKERS="$2"; shift 2 ;;
        --out-dir) OUT_DIR="$2"; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

case "$MODE" in
    quiet|same-core|isolated) ;;
    *) echo "Invalid mode: $MODE" >&2; usage; exit 1 ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mkdir -p "$OUT_DIR"

if ! command -v latency_probe >/dev/null 2>&1; then
    if [[ -x "$SCRIPT_DIR/build/timing_demo/latency_probe" ]]; then
        LATENCY_PROBE="$SCRIPT_DIR/build/timing_demo/latency_probe"
    elif [[ -x "$SCRIPT_DIR/build/latency_probe" ]]; then
        LATENCY_PROBE="$SCRIPT_DIR/build/latency_probe"
    else
        echo "latency_probe not found. Build/install it first." >&2
        exit 1
    fi
else
    LATENCY_PROBE="latency_probe"
fi

if ! command -v plot_latency.py >/dev/null 2>&1; then
    if [[ -x "$SCRIPT_DIR/plot_latency.py" ]]; then
        PLOT_LATENCY="$SCRIPT_DIR/plot_latency.py"
    else
        echo "plot_latency.py not found." >&2
        exit 1
    fi
else
    PLOT_LATENCY="plot_latency.py"
fi

if [[ "$MODE" != "quiet" ]]; then
    if ! command -v stress-ng >/dev/null 2>&1; then
        echo "stress-ng not found. Install stress-ng or run --mode quiet." >&2
        exit 1
    fi

    if ! command -v taskset >/dev/null 2>&1; then
        echo "taskset not found. CPU affinity scenarios require taskset." >&2
        exit 1
    fi
fi

case "$MODE" in
    quiet)
        OUTPUT_BASE="quiet"
        ;;
    same-core)
        OUTPUT_BASE="same_core_stress"
        STRESS_CPU="$PROBE_CPU"
        ;;
    isolated)
        OUTPUT_BASE="isolated_core_stress"
        if [[ "$PROBE_CPU" == "$STRESS_CPU" ]]; then
            echo "warning: isolated mode uses the same CPU for probe and stress." >&2
            echo "         Use different CPUs to demonstrate isolation." >&2
        fi
        ;;
esac

CSV_PATH="$OUT_DIR/${OUTPUT_BASE}.csv"
PNG_PATH="$OUT_DIR/${OUTPUT_BASE}.png"

echo "Scenario:       $MODE"
echo "Period:         ${PERIOD_MS} ms"
echo "Samples:        $SAMPLES"
echo "Work:           ${WORK_US} us"
echo "Probe CPU:      $PROBE_CPU"
echo "Stress CPU:     $STRESS_CPU"
echo "Stress workers: $STRESS_WORKERS"
echo "Output CSV:     $CSV_PATH"
echo "Output PNG:     $PNG_PATH"
echo

STRESS_PID=""

cleanup() {
    if [[ -n "$STRESS_PID" ]]; then
        kill "$STRESS_PID" >/dev/null 2>&1 || true
        wait "$STRESS_PID" >/dev/null 2>&1 || true
    fi
}

trap cleanup EXIT

if [[ "$MODE" != "quiet" ]]; then
    echo "Starting stress-ng..."
    taskset -c "$STRESS_CPU" \
        stress-ng \
        --cpu "$STRESS_WORKERS" \
        --timeout "$STRESS_TIMEOUT" \
        --metrics-brief &
    STRESS_PID="$!"

    sleep 1
fi

if [[ "$MODE" == "quiet" ]]; then
    "$LATENCY_PROBE" \
        --period-ms "$PERIOD_MS" \
        --samples "$SAMPLES" \
        --work-us "$WORK_US" \
        --output "$CSV_PATH"
else
    taskset -c "$PROBE_CPU" \
        "$LATENCY_PROBE" \
        --period-ms "$PERIOD_MS" \
        --samples "$SAMPLES" \
        --work-us "$WORK_US" \
        --output "$CSV_PATH"
fi

"$PLOT_LATENCY" \
    "$CSV_PATH" \
    "$PNG_PATH" \
    --period-us "$((PERIOD_MS * 1000))" \
    --title "$MODE"

echo
echo "Scenario complete."
