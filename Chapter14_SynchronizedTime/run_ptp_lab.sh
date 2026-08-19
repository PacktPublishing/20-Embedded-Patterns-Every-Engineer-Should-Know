#!/usr/bin/env bash
set -euo pipefail

SESSION="ch14-ptp"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BOB_DIR="${SCRIPT_DIR}"
DEV_DIR="${REPO_ROOT}"

BOB_MONITOR="/vagrant/build/ch14_clock_monitor"
DEV_MONITOR="/workspace/Chapter14_SynchronizedTime/build/ch14_clock_monitor"

BOB_IFACE="enp0s8"
DEV_IFACE="enp0s8"

usage()
{
    cat <<EOF
Usage:
    $0          Start/attach to the Chapter 14 PTP lab
    $0 stop     Stop the lab and restore Dev network time
    $0 attach   Attach to an existing lab session
EOF
}

require_command()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command not found: $1" >&2
        exit 1
    fi
}

stop_lab()
{
    echo "Stopping Chapter 14 PTP lab..."

    tmux kill-session -t "${SESSION}" 2>/dev/null || true

    (
        cd "${BOB_DIR}"
        vagrant ssh -c \
            "sudo pkill ptp4l 2>/dev/null || true" \
            >/dev/null 2>&1 || true
    )

    (
        cd "${DEV_DIR}"
        vagrant ssh -c \
            "sudo pkill ptp4l 2>/dev/null || true; \
             sudo timedatectl set-ntp true" \
            >/dev/null 2>&1 || true
    )

    echo "Lab stopped."
    echo "Dev network time synchronization restored."
}

attach_lab()
{
    if ! tmux has-session -t "${SESSION}" 2>/dev/null; then
        echo "No ${SESSION} tmux session is running." >&2
        exit 1
    fi

    exec tmux attach-session -t "${SESSION}"
}

case "${1:-start}" in
    stop)
        stop_lab
        exit 0
        ;;

    attach)
        attach_lab
        ;;

    start)
        ;;

    *)
        usage
        exit 1
        ;;
esac

require_command vagrant
require_command tmux
require_command cmake

#
# Build the monitor once on the host. The Chapter 14 directory is shared
# into both VMs, so both machines execute the same binary.
#
if [[ ! -x "${SCRIPT_DIR}/build/ch14_clock_monitor" ]]; then
    echo "Building ch14_clock_monitor..."
    cmake -S "${SCRIPT_DIR}" -B "${SCRIPT_DIR}/build"
    cmake --build "${SCRIPT_DIR}/build"
fi

#
# Bring up both VMs.
#
echo "Starting Dev..."
(
    cd "${DEV_DIR}"
    vagrant up
)

echo "Starting Chronos..."
(
    cd "${BOB_DIR}"
    vagrant up
)

#
# Stop anything that would compete with PTP and remove stale ptp4l
# processes from previous runs.
#
echo "Preparing Dev..."
(
    cd "${DEV_DIR}"

    vagrant ssh -c \
        "sudo pkill ptp4l 2>/dev/null || true; \
         sudo timedatectl set-ntp false"
)

echo "Preparing Chronos..."
(
    cd "${BOB_DIR}"

    vagrant ssh -c \
        "sudo pkill ptp4l 2>/dev/null || true; \
         sudo timedatectl set-ntp false"
)

#
# Remove an old tmux session if one survived a previous experiment.
#
tmux kill-session -t "${SESSION}" 2>/dev/null || true

#
# Commands used in the four main panes.
#
BOB_CLOCK_CMD="cd '${BOB_DIR}' && \
    vagrant ssh -c '${BOB_MONITOR} 500'"

DEV_CLOCK_CMD="cd '${DEV_DIR}' && \
    vagrant ssh -c '${DEV_MONITOR} 500'"

BOB_PTP_CMD="cd '${BOB_DIR}' && \
    vagrant ssh -c 'sudo ptp4l -2 -S \
        -i ${BOB_IFACE} \
        -m \
        --priority1 100'"

DEV_PTP_CMD="cd '${DEV_DIR}' && \
    vagrant ssh -c 'sleep 2; sudo ptp4l -2 -S -s \
        -i ${DEV_IFACE} \
        -m \
        --step_threshold 1.0'"

#
# Create the main four-pane lab window.
#
tmux new-session \
    -d \
    -s "${SESSION}" \
    -n lab \
    "${BOB_CLOCK_CMD}"

BOB_CLOCK_PANE="$(tmux display-message -p -t "${SESSION}:lab" '#{pane_id}')"

DEV_CLOCK_PANE="$(
    tmux split-window \
        -h \
        -t "${BOB_CLOCK_PANE}" \
        -P \
        -F '#{pane_id}' \
        "${DEV_CLOCK_CMD}"
)"

BOB_PTP_PANE="$(
    tmux split-window \
        -v \
        -t "${BOB_CLOCK_PANE}" \
        -P \
        -F '#{pane_id}' \
        "${BOB_PTP_CMD}"
)"

DEV_PTP_PANE="$(
    tmux split-window \
        -v \
        -t "${DEV_CLOCK_PANE}" \
        -P \
        -F '#{pane_id}' \
        "${DEV_PTP_CMD}"
)"

tmux select-pane -t "${BOB_CLOCK_PANE}" -T "CHRONOS — WALL / MONOTONIC"
tmux select-pane -t "${DEV_CLOCK_PANE}" -T "DEV — WALL / MONOTONIC"
tmux select-pane -t "${BOB_PTP_PANE}"   -T "CHRONOS — PTP GRANDMASTER"
tmux select-pane -t "${DEV_PTP_PANE}"   -T "DEV — PTP CLIENT"

tmux set-option \
    -t "${SESSION}:lab" \
    pane-border-status top

tmux set-option \
    -t "${SESSION}:lab" \
    pane-border-format ' #{pane_title} '

#
# Second window: interactive shells on Chronos and Dev.
#
tmux new-window \
    -t "${SESSION}" \
    -n control \
    "cd '${BOB_DIR}' && exec vagrant ssh"

BOB_CONTROL_PANE="$(
    tmux display-message \
        -p \
        -t "${SESSION}:control" \
        '#{pane_id}'
)"

DEV_CONTROL_PANE="$(
    tmux split-window \
        -h \
        -t "${BOB_CONTROL_PANE}" \
        -P \
        -F '#{pane_id}' \
        "cd '${DEV_DIR}' && exec vagrant ssh"
)"

tmux select-pane -t "${BOB_CONTROL_PANE}" -T "BOB — CONTROL"
tmux select-pane -t "${DEV_CONTROL_PANE}" -T "DEV — CONTROL"

tmux set-option \
    -t "${SESSION}:control" \
    pane-border-status top

tmux set-option \
    -t "${SESSION}:control" \
    pane-border-format ' #{pane_title} '

#
# Start with the four-pane display.
#
tmux select-window -t "${SESSION}:lab"

cat <<EOF

Chapter 14 PTP lab is running.

tmux windows:

    Ctrl-b 0    PTP/clock display
    Ctrl-b 1    Chronos and Dev control shells

Useful commands in the Chronos control shell:

    date

    sudo date --set="\$(date --date='+120 seconds' \
        '+%Y-%m-%d %H:%M:%S')"

Useful command in the Dev control shell:

    sudo pmc -u -b 0 'GET TIME_STATUS_NP'

Watch what happens when Chronos's wall clock moves:

    * Chronos's system_clock jumps.
    * Dev detects the PTP offset.
    * Dev's CLOCK_REALTIME follows Chronos.
    * Both steady_clock elapsed counters continue normally.

When finished:

    ./run_ptp_lab.sh stop

EOF

exec tmux attach-session -t "${SESSION}"
