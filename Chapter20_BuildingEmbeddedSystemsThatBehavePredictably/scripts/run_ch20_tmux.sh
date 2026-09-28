#!/usr/bin/env bash
set -euo pipefail
session=ch20_weather
bin_dir="${HOME}/bin"
state_file="${CH20_STATE_FILE:-${HOME}/ch20_weather.state}"
command -v tmux >/dev/null || { echo 'Install tmux to run this launcher.' >&2; exit 1; }
for program in weather_device temperature_sensor telemetry_listener command_client; do
    [[ -x "${bin_dir}/ch20_${program}" ]] || { echo "Missing ${bin_dir}/ch20_${program}; run cmake --install first." >&2; exit 1; }
done
# Always rebuild this lab session so an older four-window layout cannot persist.
# This restarts the four demo processes; settings saved to disk remain available.
if tmux has-session -t "=${session}" 2>/dev/null; then
    tmux kill-session -t "=${session}"
fi
tmux new-session -d -s "$session" -n weather
listener_pane=$(tmux display-message -p -t "${session}:weather" '#{pane_id}')
# Split once horizontally, then split each half vertically: four visible panes.
device_pane=$(tmux split-window -d -h -P -F '#{pane_id}' -t "$listener_pane")
sensor_pane=$(tmux split-window -d -v -P -F '#{pane_id}' -t "$listener_pane")
client_pane=$(tmux split-window -d -v -P -F '#{pane_id}' -t "$device_pane")
tmux select-layout -t "${session}:weather" tiled
panes=$(tmux list-panes -t "${session}:weather" -F '#{pane_id}' | wc -l)
if [[ "$panes" -ne 4 ]]; then
    echo "Expected 4 visible panes, found ${panes}." >&2
    exit 1
fi
tmux send-keys -t "$listener_pane" "${bin_dir}/ch20_telemetry_listener" C-m
tmux send-keys -t "$device_pane" "${bin_dir}/ch20_weather_device --state-file $(printf '%q' "$state_file")" C-m
tmux send-keys -t "$sensor_pane" "${bin_dir}/ch20_temperature_sensor --random-anomalies" C-m
tmux send-keys -t "$client_pane" "${bin_dir}/ch20_command_client" C-m
tmux select-pane -t "$client_pane"
tmux select-window -t "${session}:weather"
tmux attach-session -t "=${session}"
