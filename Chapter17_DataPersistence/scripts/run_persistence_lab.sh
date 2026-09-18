#!/usr/bin/env bash
set -euo pipefail

build_dir=${1:-build}
image=${2:-parameters.pimg}

"${build_dir}/ch17_weather_device" \
    --image "${image}" \
    --set-reporting-ms 2500 \
    --set-temperature-limit 55 \
    --set-operating-mode service \
    --save

"${build_dir}/ch17_image_inspector" "${image}"
"${build_dir}/ch17_weather_device" --image "${image}"
