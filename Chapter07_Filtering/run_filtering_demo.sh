#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

set -euo pipefail

output_directory="${1:-filtering_demo_output}"
script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

require_command()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command not found: $1" >&2
        exit 1
    fi
}

require_command measurement_generator
require_command filter_measurements
require_command python3

mkdir -p "$output_directory"

measurement_generator \
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
    --output "$output_directory/measurements.csv"

filter_measurements \
    "$output_directory/measurements.csv" \
    --minimum 0.0 \
    --maximum 50.0 \
    --maximum-rate 2.0 \
    --moving-average-window 5 \
    --exponential-alpha 0.1 \
    --output "$output_directory/filtered_measurements.csv"

python3 "$script_directory/plot_filtering.py" \
    "$output_directory/filtered_measurements.csv" \
    --output "$output_directory/filtering_comparison.png"

echo "Filtering demonstration complete: $output_directory"
