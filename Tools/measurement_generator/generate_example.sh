#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${script_dir}/build"

cmake -S "${script_dir}" -B "${build_dir}"
cmake --build "${build_dir}"

"${build_dir}/measurement_generator" \
    --count 140 \
    --minimum 22.0 \
    --maximum 30.0 \
    --profile triangle \
    --sample-rate 1.0 \
    --noise-stddev 0.25 \
    --spike-frequency 0.02 \
    --missing-frequency 0.01 \
    --spike-minimum 20.0 \
    --spike-maximum 30.0 \
    --seed 12345 \
    --output "${script_dir}/measurements.csv"

python3 "${script_dir}/plot_measurements.py" \
    "${script_dir}/measurements.csv" \
    --output "${script_dir}/noisy_measurements.png"
