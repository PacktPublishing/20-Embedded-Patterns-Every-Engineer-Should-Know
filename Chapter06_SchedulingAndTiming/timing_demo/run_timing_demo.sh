#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

set -euo pipefail

MODE="all"
PERIOD_MS=10
SAMPLES=2000
WORK_US=500
PROBE_CPU=1
STRESS_CPU=2
STRESS_TIMEOUT="25s"
STRESS_WORKERS=1
OUT_DIR="results/ch06"
SESSION="ch06_timing_demo"

usage() {
    cat <<EOF
Usage:
  $0 [options]

Options:
  --mode quiet|same-core|isolated|all
  --period-ms N
  --samples N
  --work-us N
  --probe-cpu N
  --stress-cpu N
  --stress-timeout DURATION
  --stress-workers N
  --out-dir DIR
  --session NAME
  --help

Examples:
  $0 --mode all
  $0 --mode same-core --probe-cpu 1
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
        --session) SESSION="$2"; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

case "$MODE" in
    quiet|same-core|isolated|all) ;;
    *) echo "Invalid mode: $MODE" >&2; usage; exit 1 ;;
esac

if ! command -v tmux >/dev/null 2>&1; then
    echo "tmux not found. Install tmux before running the timing demo." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "$OUT_DIR" != /* ]]; then
    OUT_DIR="$(pwd)/$OUT_DIR"
fi

mkdir -p "$OUT_DIR"

RUNNER="$OUT_DIR/run_selected_scenarios.sh"

cat > "$RUNNER" <<EOF
#!/usr/bin/env bash
set -euo pipefail

cd "$SCRIPT_DIR"

echo "Chapter 6 timing demo"
echo "Mode:       $MODE"
echo "Period:     ${PERIOD_MS} ms"
echo "Samples:    $SAMPLES"
echo "Work:       ${WORK_US} us"
echo "Probe CPU:  $PROBE_CPU"
echo "Stress CPU: $STRESS_CPU"
echo "Out dir:    $OUT_DIR"
echo
EOF

add_scenario() {
    local scenario="$1"
    local scenario_probe_cpu="$2"
    local scenario_stress_cpu="$3"

    cat >> "$RUNNER" <<EOF

echo "============================================================"
echo "Running scenario: $scenario"
echo "============================================================"

./run_latency_scenario.sh \\
    --mode "$scenario" \\
    --period-ms "$PERIOD_MS" \\
    --samples "$SAMPLES" \\
    --work-us "$WORK_US" \\
    --probe-cpu "$scenario_probe_cpu" \\
    --stress-cpu "$scenario_stress_cpu" \\
    --stress-timeout "$STRESS_TIMEOUT" \\
    --stress-workers "$STRESS_WORKERS" \\
    --out-dir "$OUT_DIR"

EOF
}

if [[ "$MODE" == "all" ]]; then
    add_scenario "quiet" "$PROBE_CPU" "$STRESS_CPU"
    add_scenario "same-core" "$PROBE_CPU" "$PROBE_CPU"
    add_scenario "isolated" "$PROBE_CPU" "$STRESS_CPU"
else
    if [[ "$MODE" == "same-core" ]]; then
        add_scenario "$MODE" "$PROBE_CPU" "$PROBE_CPU"
    else
        add_scenario "$MODE" "$PROBE_CPU" "$STRESS_CPU"
    fi
fi

cat >> "$RUNNER" <<EOF

echo
echo "Generated files:"
find "$OUT_DIR" -maxdepth 1 -type f | sort

echo
echo "Timing demo complete."
echo "Press Enter to leave this pane open."
read -r _
EOF

chmod +x "$RUNNER"

if tmux has-session -t "$SESSION" 2>/dev/null; then
    tmux kill-session -t "$SESSION"
fi

tmux new-session -d -s "$SESSION" -n timing

tmux split-window -h -t "$SESSION:0"
tmux split-window -v -t "$SESSION:0.1"

tmux send-keys -t "$SESSION:0.0" "cd \"$SCRIPT_DIR\" && \"$RUNNER\"" C-m

if command -v htop >/dev/null 2>&1; then
    tmux send-keys -t "$SESSION:0.1" "htop" C-m
else
    tmux send-keys -t "$SESSION:0.1" "top" C-m
fi

tmux send-keys -t "$SESSION:0.2" "cd \"$SCRIPT_DIR\" && watch -n 1 'ls -lh \"$OUT_DIR\" 2>/dev/null || true'" C-m

tmux select-pane -t "$SESSION:0.0"
tmux attach-session -t "$SESSION"
