#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${project_dir}/build"
output_dir="${project_dir}/output"

cmake -S "${project_dir}" -B "${build_dir}"
cmake --build "${build_dir}"
ctest --test-dir "${build_dir}" --output-on-failure
mkdir -p "${output_dir}"

for scenario in open-loop p pi pid windup disturbance; do
    csv_file="${output_dir}/${scenario}.csv"
    "${build_dir}/smoker_sim" "${scenario}" "${csv_file}"
    python3 "${project_dir}/scripts/plot_pid_response.py" "${csv_file}"
done

echo "Results are in ${output_dir}"
