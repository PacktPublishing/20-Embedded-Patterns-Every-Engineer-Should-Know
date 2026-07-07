#!/usr/bin/env bash
set -euo pipefail

SESSION="${1:-ch03-readiness}"
UDP_PORT="${UDP_PORT:-9000}"
STATS_EVERY="${STATS_EVERY:-5}"

if ! command -v tmux >/dev/null 2>&1; then
    echo "tmux is not installed."
    echo "Install it with: sudo apt install tmux"
    exit 1
fi

if ! command -v socat >/dev/null 2>&1; then
    echo "socat is not installed."
    echo "If you are using the book's Vagrant environment, reprovision the VM."
    echo "Otherwise install it with: sudo apt install socat"
    exit 1
fi

if ! command -v ch03_nmea_monitor >/dev/null 2>&1; then
    echo "ch03_nmea_monitor was not found on PATH."
    echo "Build it and copy it to your bin directory, or add its build directory to PATH."
    exit 1
fi

if ! command -v run_nmea_sources.sh >/dev/null 2>&1; then
    echo "run_nmea_sources.sh was not found on PATH."
    echo "Copy it to your bin directory, or add its directory to PATH."
    exit 1
fi

COLS="$(tput cols)"
LINES="$(tput lines)"

if (( COLS < 90 || LINES < 35 )); then
    echo "This demo layout works best with a terminal at least 100 columns x 35 lines."
    echo "Current size: ${COLS} columns x ${LINES} lines"
    echo
    echo "Resize the terminal and run the script again."
    exit 1
fi

if tmux has-session -t "$SESSION" 2>/dev/null; then
    echo "Attaching to existing tmux session: $SESSION"
    tmux attach-session -t "$SESSION"
    exit 0
fi

SOCAT_LOG="$(mktemp /tmp/ch03-socat-pty.XXXXXX.log)"

tmux new-session -d -s "$SESSION" -n readiness -x "$COLS" -y "$LINES"

# Three stacked panes:
#   pane 0: socat
#   pane 1: nmea_sim sources
#   pane 2: nmea_monitor
tmux split-window -v -t "$SESSION:0.0"
tmux split-window -v -t "$SESSION:0.1"

tmux select-layout -t "$SESSION:0" even-vertical

# Keep setup panes small and leave the monitor pane larger.
tmux resize-pane -t "$SESSION:0.0" -y 7 2>/dev/null || true
tmux resize-pane -t "$SESSION:0.1" -y 11 2>/dev/null || true

tmux select-pane -t "$SESSION:0.0" -T "socat"
tmux select-pane -t "$SESSION:0.1" -T "nmea_sim"
tmux select-pane -t "$SESSION:0.2" -T "nmea_monitor"

# Start socat in the top pane. Its output is displayed in the pane and also
# written to a temporary log so this script can discover the PTY names.
tmux send-keys -t "$SESSION:0.0" \
    "cd \"\$HOME\" && clear && socat -d -d pty,raw,echo=0 pty,raw,echo=0 2>&1 | tee -a \"$SOCAT_LOG\"" C-m

PTY_A=""
PTY_B=""

# Wait up to 5 seconds for socat to print both PTY names.
for _ in {1..50}; do
    mapfile -t PTYS < <(
        grep -oE '/dev/pts/[0-9]+' "$SOCAT_LOG" 2>/dev/null \
            | awk '!seen[$0]++' \
            | head -n 2
    )

    if (( ${#PTYS[@]} >= 2 )); then
        PTY_A="${PTYS[0]}"
        PTY_B="${PTYS[1]}"
        break
    fi

    sleep 0.1
done

if [[ -z "$PTY_A" || -z "$PTY_B" ]]; then
    echo "Failed to discover PTY names from socat output."
    echo "socat log: $SOCAT_LOG"
    echo
    echo "Attaching to tmux so you can inspect the socat pane."
    tmux attach-session -t "$SESSION"
    exit 1
fi

# Start the monitor first so it is listening before the sources send data.
tmux send-keys -t "$SESSION:0.2" \
    "cd \"\$HOME\" && clear && ch03_nmea_monitor --udp-port \"$UDP_PORT\" --uart \"$PTY_A\" --stats-every \"$STATS_EVERY\"" C-m

sleep 0.5

# Start the simulated sources.
tmux send-keys -t "$SESSION:0.1" \
    "cd \"\$HOME\" && clear && run_nmea_sources.sh \"$PTY_B\"" C-m

tmux select-pane -t "$SESSION:0.2"
tmux attach-session -t "$SESSION"
