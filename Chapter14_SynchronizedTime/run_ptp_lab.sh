#!/usr/bin/env bash

set -euo pipefail

SESSION="ch14-ptp"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

CHRONOS_DIR="${SCRIPT_DIR}"
DEV_DIR="${REPO_ROOT}"

CHRONOS_MONITOR="/vagrant/build/ch14_clock_monitor"
DEV_MONITOR="/workspace/Chapter14_SynchronizedTime/build/ch14_clock_monitor"

CHRONOS_IFACE="enp0s8"
DEV_IFACE="enp0s8"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

run_in_dir()
{
    local dir="$1"
    shift

    (
        cd "$dir"
        "$@"
    )
}

kill_ptp()
{
    run_in_dir "$CHRONOS_DIR" \
        vagrant ssh -c "sudo pkill -x ptp4l 2>/dev/null || true" \
        >/dev/null 2>&1 || true

    run_in_dir "$DEV_DIR" \
        vagrant ssh -c "sudo pkill -x ptp4l 2>/dev/null || true" \
        >/dev/null 2>&1 || true
}

stop_lab()
{
    tmux kill-session -t "$SESSION" 2>/dev/null || true

    kill_ptp

    # Restore normal time synchronization on the development VM.
    run_in_dir "$DEV_DIR" \
        vagrant ssh -c "sudo timedatectl set-ntp true" \
        >/dev/null 2>&1 || true

    echo "PTP lab stopped."
}

halt_lab()
{
    stop_lab

    echo "Halting Chronos..."
    run_in_dir "$CHRONOS_DIR" vagrant halt

    echo "Halting Dev..."
    run_in_dir "$DEV_DIR" vagrant halt

    echo "Virtual machines halted."
}

attach_lab()
{
    if ! tmux has-session -t "$SESSION" 2>/dev/null; then
        echo "No ${SESSION} tmux session is running."
        exit 1
    fi

    tmux attach-session -t "$SESSION"
}

# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------

case "${1:-start}" in
    start)
        ;;
    stop)
        stop_lab
        exit 0
        ;;
    halt)
        halt_lab
        exit 0
        ;;
    attach)
        attach_lab
        exit 0
        ;;
    *)
        echo "Usage: $0 [start|stop|halt|attach]"
        exit 1
        ;;
esac

# ---------------------------------------------------------------------------
# Preconditions
# ---------------------------------------------------------------------------

for command in tmux vagrant cmake; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "Required command not found: $command"
        exit 1
    fi
done

if tmux has-session -t "$SESSION" 2>/dev/null; then
    echo "Lab session already exists. Attaching..."
    tmux attach-session -t "$SESSION"
    exit 0
fi

cols="$(tput cols 2>/dev/null || echo 0)"
lines="$(tput lines 2>/dev/null || echo 0)"

if (( cols < 140 || lines < 40 )); then
    echo
    echo "For best results, enlarge this terminal window."
    echo "Current size: ${cols} columns x ${lines} lines"
    echo
    sleep 2
fi

# ---------------------------------------------------------------------------
# Build the clock monitor
# ---------------------------------------------------------------------------

if [[ ! -x "${SCRIPT_DIR}/build/ch14_clock_monitor" ]]; then
    echo "Building Chapter 14 clock monitor..."

    cmake \
        -S "$SCRIPT_DIR" \
        -B "${SCRIPT_DIR}/build"

    cmake --build "${SCRIPT_DIR}/build"
fi

# ---------------------------------------------------------------------------
# Start the VMs
# ---------------------------------------------------------------------------

echo "Starting Dev..."
run_in_dir "$DEV_DIR" vagrant up

echo "Starting Chronos..."
run_in_dir "$CHRONOS_DIR" vagrant up

# ---------------------------------------------------------------------------
# Prepare time services
# ---------------------------------------------------------------------------

echo "Preparing Chronos..."
run_in_dir "$CHRONOS_DIR" \
    vagrant ssh -c \
    "sudo pkill -x ptp4l 2>/dev/null || true;
     sudo timedatectl set-ntp false"

echo "Preparing Dev..."
run_in_dir "$DEV_DIR" \
    vagrant ssh -c \
    "sudo pkill -x ptp4l 2>/dev/null || true;
     sudo timedatectl set-ntp false"

# ---------------------------------------------------------------------------
# Create tmux lab window
# ---------------------------------------------------------------------------

tmux new-session -d -s "$SESSION" -n lab

tmux set-option -t "$SESSION" pane-border-status top
tmux set-option -t "$SESSION" \
    pane-border-format ' #{pane_title} '

# Keep panes around if their commands ever terminate.
tmux set-window-option -t "${SESSION}:lab" remain-on-exit on

CHRONOS_CLOCK_PANE="$(
    tmux display-message \
        -p \
        -t "${SESSION}:lab.0" \
        '#{pane_id}'
)"

DEV_CLOCK_PANE="$(
    tmux split-window \
        -h \
        -t "$CHRONOS_CLOCK_PANE" \
        -P \
        -F '#{pane_id}'
)"

CHRONOS_PTP_PANE="$(
    tmux split-window \
        -v \
        -t "$CHRONOS_CLOCK_PANE" \
        -P \
        -F '#{pane_id}'
)"

DEV_PTP_PANE="$(
    tmux split-window \
        -v \
        -t "$DEV_CLOCK_PANE" \
        -P \
        -F '#{pane_id}'
)"

tmux select-pane \
    -t "$CHRONOS_CLOCK_PANE" \
    -T "CHRONOS — WALL / MONOTONIC"

tmux select-pane \
    -t "$DEV_CLOCK_PANE" \
    -T "DEV — WALL / MONOTONIC"

tmux select-pane \
    -t "$CHRONOS_PTP_PANE" \
    -T "CHRONOS — PTP GRANDMASTER"

tmux select-pane \
    -t "$DEV_PTP_PANE" \
    -T "DEV — PTP CLIENT"

# ---------------------------------------------------------------------------
# Start clock monitors
#
# These commands are sent to normal shell panes instead of being used as
# the pane's process. If a command fails, the pane remains visible and
# displays the exit status rather than disappearing and destroying the
# layout.
# ---------------------------------------------------------------------------

tmux send-keys \
    -t "$CHRONOS_CLOCK_PANE" \
    "cd '$CHRONOS_DIR' && vagrant ssh -c '$CHRONOS_MONITOR 500'; rc=\$?; echo; echo \"*** CHRONOS CLOCK EXITED: \$rc ***\"" \
    C-m

tmux send-keys \
    -t "$DEV_CLOCK_PANE" \
    "cd '$DEV_DIR' && vagrant ssh -c '$DEV_MONITOR 500'; rc=\$?; echo; echo \"*** DEV CLOCK EXITED: \$rc ***\"" \
    C-m

# ---------------------------------------------------------------------------
# Start PTP
# ---------------------------------------------------------------------------

tmux send-keys \
    -t "$CHRONOS_PTP_PANE" \
    "cd '$CHRONOS_DIR' && vagrant ssh -c 'sudo ptp4l -2 -S -i $CHRONOS_IFACE -m --priority1 100'; rc=\$?; echo; echo \"*** CHRONOS PTP EXITED: \$rc ***\"" \
    C-m

tmux send-keys \
    -t "$DEV_PTP_PANE" \
    "sleep 2; cd '$DEV_DIR' && vagrant ssh -c 'sudo ptp4l -2 -S -s -i $DEV_IFACE -m --step_threshold 1.0'; rc=\$?; echo; echo \"*** DEV PTP EXITED: \$rc ***\"" \
    C-m

# ---------------------------------------------------------------------------
# Create control window
# ---------------------------------------------------------------------------

tmux new-window \
    -d \
    -t "$SESSION" \
    -n control

tmux set-window-option \
    -t "${SESSION}:control" \
    remain-on-exit on

CHRONOS_CONTROL_PANE="$(
    tmux display-message \
        -p \
        -t "${SESSION}:control.0" \
        '#{pane_id}'
)"

DEV_CONTROL_PANE="$(
    tmux split-window \
        -h \
        -t "$CHRONOS_CONTROL_PANE" \
        -P \
        -F '#{pane_id}'
)"

tmux select-pane \
    -t "$CHRONOS_CONTROL_PANE" \
    -T "CHRONOS — CONTROL"

tmux select-pane \
    -t "$DEV_CONTROL_PANE" \
    -T "DEV — CONTROL"

tmux send-keys \
    -t "$CHRONOS_CONTROL_PANE" \
    "cd '$CHRONOS_DIR' && vagrant ssh" \
    C-m

tmux send-keys \
    -t "$DEV_CONTROL_PANE" \
    "cd '$DEV_DIR' && vagrant ssh" \
    C-m

# ---------------------------------------------------------------------------
# Display lab
# ---------------------------------------------------------------------------

tmux select-window -t "${SESSION}:lab"

echo
echo "Chapter 14 PTP lab started."
echo
echo "  Ctrl-b 0    Lab display"
echo "  Ctrl-b 1    Control shells"
echo
echo "On Chronos, move time ahead two minutes:"
echo
echo "  sudo date --set=\"\$(date --date='+120 seconds' '+%Y-%m-%d %H:%M:%S')\""
echo
echo "Move it back two minutes:"
echo
echo "  sudo date --set=\"\$(date --date='-120 seconds' '+%Y-%m-%d %H:%M:%S')\""
echo
echo "Stop lab:"
echo
echo "  $0 stop"
echo
echo "Stop lab and halt both VMs:"
echo
echo "  $0 halt"
echo

tmux attach-session -t "$SESSION"
