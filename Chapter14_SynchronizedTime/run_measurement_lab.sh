#!/usr/bin/env bash
set -euo pipefail

SESSION="ch14-measurements"

#
# This script is intended to be run while the current directory is
# Chapter14_SynchronizedTime.  It may itself be installed in $HOME/bin,
# so do not derive the repository location from BASH_SOURCE.
#
CHAPTER_DIR="$(pwd)"
REPO_ROOT="$(cd "${CHAPTER_DIR}/.." && pwd)"

CHRONOS_DIR="${CHAPTER_DIR}"
DEV_DIR="${REPO_ROOT}"

CHRONOS_IFACE="enp0s8"
DEV_IFACE="enp0s8"

DEV_IP="192.168.56.15"
UDP_PORT="45014"

MATCH_WINDOW_MS="40"
LOCAL_PERIOD_MS="333"
REMOTE_PERIOD_MS="100"

#
# The repository is shared differently in the two VMs.
#
# Dev:
#   repository root -> /workspace
#
# Chronos:
#   Chapter14_SynchronizedTime -> /vagrant
#
CHRONOS_TX="/vagrant/build/ch14_wind_speed_tx"
DEV_FUSION="/workspace/Chapter14_SynchronizedTime/build/ch14_synchronized_fusion"


run_in_dir()
{
    local directory="$1"
    shift

    (
        cd "${directory}"
        "$@"
    )
}


vagrant_ssh()
{
    local directory="$1"
    shift

    run_in_dir "${directory}" vagrant ssh -c "$*"
}


check_environment()
{
    if [[ ! -f "${CHAPTER_DIR}/Vagrantfile" ]]; then
        echo "Error: run this script from Chapter14_SynchronizedTime." >&2
        exit 1
    fi

    if [[ ! -f "${REPO_ROOT}/Vagrantfile" ]]; then
        echo "Error: repository Vagrantfile not found in:" >&2
        echo "  ${REPO_ROOT}" >&2
        exit 1
    fi

    local missing=0

    if [[ ! -x "${CHAPTER_DIR}/build/ch14_wind_speed_tx" ]]; then
        echo "Missing executable:"
        echo "  ${CHAPTER_DIR}/build/ch14_wind_speed_tx"
        missing=1
    fi

    if [[ ! -x "${CHAPTER_DIR}/build/ch14_synchronized_fusion" ]]; then
        echo "Missing executable:"
        echo "  ${CHAPTER_DIR}/build/ch14_synchronized_fusion"
        missing=1
    fi

    if (( missing )); then
        echo
        echo "Build the Chapter 14 examples before running this lab."
        echo
        echo "For example:"
        echo
        echo "  cmake -S . -B build"
        echo "  cmake --build build -j"
        echo "  cmake --install build"
        echo
        exit 1
    fi
}


kill_remote_processes()
{
    #
    # Demo processes may already be gone. These commands are
    # intentionally quiet.
    #
    vagrant_ssh "${CHRONOS_DIR}" \
        "pkill -f 'ch14_wind_speed_tx' >/dev/null 2>&1 || true; \
         sudo pkill -x ptp4l >/dev/null 2>&1 || true" \
        >/dev/null 2>&1 || true

    vagrant_ssh "${DEV_DIR}" \
        "pkill -f 'ch14_synchronized_fusion' >/dev/null 2>&1 || true; \
         sudo pkill -x ptp4l >/dev/null 2>&1 || true" \
        >/dev/null 2>&1 || true
}


restore_dev_ntp()
{
    vagrant_ssh "${DEV_DIR}" \
        "sudo timedatectl set-ntp true >/dev/null 2>&1 || true" \
        >/dev/null 2>&1 || true
}


stop_lab()
{
    if tmux has-session -t "${SESSION}" 2>/dev/null; then
        tmux kill-session -t "${SESSION}"
    fi

    kill_remote_processes
    restore_dev_ntp

    echo "Lab 2 stopped. VMs are still running."
}


halt_lab()
{
    stop_lab

    echo "Halting Chronos..."
    run_in_dir "${CHRONOS_DIR}" vagrant halt || true

    echo "Halting Dev..."
    run_in_dir "${DEV_DIR}" vagrant halt || true

    echo "Both VMs halted."
}


attach_lab()
{
    exec tmux attach-session -t "${SESSION}"
}


prepare_vms()
{
    echo "Starting Dev VM..."
    run_in_dir "${DEV_DIR}" vagrant up

    echo "Starting Chronos VM..."
    run_in_dir "${CHRONOS_DIR}" vagrant up

    #
    # Remove anything left behind by an earlier run.
    #
    kill_remote_processes

    echo "Disabling ordinary NTP while PTP owns the clocks..."

    vagrant_ssh "${CHRONOS_DIR}" \
        "sudo timedatectl set-ntp false >/dev/null 2>&1 || true"

    vagrant_ssh "${DEV_DIR}" \
        "sudo timedatectl set-ntp false >/dev/null 2>&1 || true"
}


start_ptp()
{
    echo "Starting Chronos as PTP grandmaster..."

    vagrant_ssh "${CHRONOS_DIR}" \
        "sudo sh -c \
        'nohup ptp4l \
            -2 \
            -S \
            -i ${CHRONOS_IFACE} \
            -m \
            --priority1 100 \
            > /tmp/ch14_ptp_grandmaster.log 2>&1 \
            < /dev/null & \
         echo \$! > /tmp/ch14_ptp_grandmaster.pid'"

    #
    # Give the grandmaster a moment to establish itself before
    # starting the client.
    #
    sleep 1

    echo "Starting Dev as PTP client..."

    vagrant_ssh "${DEV_DIR}" \
        "sudo sh -c \
        'nohup ptp4l \
            -2 \
            -S \
            -s \
            -i ${DEV_IFACE} \
            -m \
            --step_threshold 1.0 \
            > /tmp/ch14_ptp_client.log 2>&1 \
            < /dev/null & \
         echo \$! > /tmp/ch14_ptp_client.pid'"
}


wait_for_ptp_sync()
{
    echo -n "Waiting for Dev to enter PTP SLAVE state"

    local attempt

    for attempt in $(seq 1 60); do
        if vagrant_ssh "${DEV_DIR}" \
            "sudo pmc -u -b 0 'GET PORT_DATA_SET' 2>/dev/null |
             grep -Eq 'portState[[:space:]]+SLAVE'" \
            >/dev/null 2>&1; then

            echo
            echo "PTP synchronization established."
            return 0
        fi

        echo -n "."
        sleep 0.5
    done

    echo
    echo "Timed out waiting for PTP synchronization."
    echo
    echo "Last Dev PTP messages:"

    vagrant_ssh "${DEV_DIR}" \
        "tail -n 20 /tmp/ch14_ptp_client.log 2>/dev/null || true" \
        || true

    return 1
}


create_tmux_lab()
{
    if tmux has-session -t "${SESSION}" 2>/dev/null; then
        tmux kill-session -t "${SESSION}"
    fi

    tmux new-session -d \
        -s "${SESSION}" \
        -n lab

    tmux set-option \
        -t "${SESSION}" \
        pane-border-status top

    tmux set-option \
        -t "${SESSION}" \
        pane-border-format ' #{pane_title} '

    tmux set-window-option \
        -t "${SESSION}:lab" \
        remain-on-exit on

    local chronos_pane
    chronos_pane="$(
        tmux display-message \
            -p \
            -t "${SESSION}:lab" \
            '#{pane_id}'
    )"

    local dev_pane
    dev_pane="$(
        tmux split-window \
            -h \
            -t "${chronos_pane}" \
            -P \
            -F '#{pane_id}'
    )"

    tmux select-pane \
        -t "${chronos_pane}" \
        -T "CHRONOS - REMOTE WIND SPEED"

    tmux select-pane \
        -t "${dev_pane}" \
        -T "DEV - SYNCHRONIZED FUSION"

    tmux send-keys \
        -t "${chronos_pane}" \
        "cd '${CHRONOS_DIR}' && \
         vagrant ssh -c \
         '${CHRONOS_TX} ${DEV_IP} ${UDP_PORT} ${REMOTE_PERIOD_MS}'; \
         rc=\$?; \
         echo; \
         echo '[Chronos simulator exited: status '\$rc']'; \
         exec bash" \
        C-m

    tmux send-keys \
        -t "${dev_pane}" \
        "cd '${DEV_DIR}' && \
         vagrant ssh -c \
         '${DEV_FUSION} ${UDP_PORT} ${MATCH_WINDOW_MS} ${LOCAL_PERIOD_MS}'; \
         rc=\$?; \
         echo; \
         echo '[Dev application exited: status '\$rc']'; \
         exec bash" \
        C-m

    tmux select-layout \
        -t "${SESSION}:lab" \
        even-horizontal

    tmux select-pane \
        -t "${chronos_pane}"

    echo
    echo "Lab 2 is running."
    echo
    echo "PTP logs are intentionally hidden:"
    echo "  Chronos: /tmp/ch14_ptp_grandmaster.log"
    echo "  Dev:     /tmp/ch14_ptp_client.log"
    echo
    echo "Stop the lab but leave the VMs running:"
    echo "  run_measurement_lab.sh stop"
    echo
    echo "Stop the lab and halt both VMs:"
    echo "  run_measurement_lab.sh halt"
    echo

    exec tmux attach-session -t "${SESSION}"
}


start_lab()
{
    check_environment
    prepare_vms
    start_ptp

    if ! wait_for_ptp_sync; then
        kill_remote_processes
        restore_dev_ntp
        exit 1
    fi

    create_tmux_lab
}


case "${1:-start}" in
    start)
        start_lab
        ;;

    stop)
        stop_lab
        ;;

    halt)
        halt_lab
        ;;

    attach)
        attach_lab
        ;;

    *)
        echo "Usage: $0 [start|stop|halt|attach]" >&2
        exit 2
        ;;
esac
