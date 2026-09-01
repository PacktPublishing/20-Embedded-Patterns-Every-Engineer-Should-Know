#!/usr/bin/env bash
set -euo pipefail

SESSION=${CH15_TMUX_SESSION:-ch15_monitoring_lab}
WEATHER_DEVICE=${CH15_WEATHER_DEVICE:-ch15_weather_device}
REMOTE_MONITOR=${CH15_REMOTE_MONITOR:-ch15_remote_monitor}
PORT=${CH15_PORT:-9500}

require_command()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command not found: $1" >&2
        exit 1
    fi
}

require_command tmux
require_command "$WEATHER_DEVICE"
require_command "$REMOTE_MONITOR"

tmux kill-session -t "$SESSION" 2>/dev/null || true

tmux new-session -d -s "$SESSION" -n lab \
    "exec $WEATHER_DEVICE --port $PORT"

tmux split-window -h -t "$SESSION:lab" \
    "sleep 0.5; exec $REMOTE_MONITOR --host 127.0.0.1 --port $PORT --demo"

tmux select-layout -t "$SESSION:lab" even-horizontal >/dev/null
tmux set-option -t "$SESSION" remain-on-exit on >/dev/null
tmux select-pane -t "$SESSION:lab.0"

printf 'Chapter 15 monitoring lab\n'
printf '  left pane:  Weather Device\n'
printf '  right pane: Remote Monitor\n'
printf '  tmux session: %s\n\n' "$SESSION"
printf 'Detach with Ctrl-b d. Kill the lab with:\n'
printf '  tmux kill-session -t %s\n\n' "$SESSION"

tmux attach-session -t "$SESSION"
