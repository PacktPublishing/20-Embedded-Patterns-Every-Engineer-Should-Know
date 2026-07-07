#!/usr/bin/env bash
set -euo pipefail

UART_DEVICE="${1:-}"

if [[ -z "$UART_DEVICE" ]]; then
    echo "Usage: $0 <uart-pty>"
    echo
    echo "Example:"
    echo "  $0 /dev/pts/9"
    exit 1
fi

UDP_TARGET="127.0.0.1:9000"

echo "Starting simulated NMEA sources"
echo "  UDP target:  ${UDP_TARGET}"
echo "  UART target: ${UART_DEVICE}"
echo
echo "Press Ctrl-C to stop."

cleanup()
{
    echo
    echo "Stopping simulated NMEA sources..."
    jobs -p | xargs -r kill
}

trap cleanup EXIT INT TERM

nmea_sim --udp "${UDP_TARGET}" --temp-hz 1 --pressure-hz 1 &
nmea_sim --uart "${UART_DEVICE}" --temp-hz 1 &

wait
