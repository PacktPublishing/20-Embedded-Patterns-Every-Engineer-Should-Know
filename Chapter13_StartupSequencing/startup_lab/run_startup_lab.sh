#!/usr/bin/env bash
set -u

LAB=${CH13_STARTUP_LAB:-ch13_startup_lab}

run_case()
{
    local title=$1
    shift

    printf '\n============================================================\n'
    printf '%s\n' "$title"
    printf '============================================================\n'

    "$LAB" "$@" || true
}

run_case "1. Normal startup"
run_case "2. Optional telemetry failure" \
    --fail_component telemetry
run_case "3. Required sensor failure" \
    --fail_component sensor
run_case "4. Startup timeout" \
    --failure_timeout 500
