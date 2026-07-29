#!/usr/bin/env bash

set -euo pipefail

project_dir="$(pwd)"

if [[ ! -f "${project_dir}/CMakeLists.txt" ||
      ! -f "${project_dir}/scripts/plot_pid_response.py" ]]; then
    echo "Error: run_pid_demo.sh must be run from the smoker_pid directory." >&2
    echo "Current directory: ${project_dir}" >&2
    exit 1
fi

build_dir="${project_dir}/build"
output_dir="${project_dir}/output"
plot_script="${project_dir}/scripts/plot_pid_response.py"

cmake -S "${project_dir}" -B "${build_dir}"
cmake --build "${build_dir}"

ctest \
    --test-dir "${build_dir}" \
    --output-on-failure

mkdir -p "${output_dir}"

for scenario in open-loop p pi pid windup disturbance
do
    csv_file="${output_dir}/${scenario}.csv"
    png_file="${output_dir}/${scenario}.png"

    "${build_dir}/smoker_sim" \
        "${scenario}" \
        "${csv_file}"

    python3 "${plot_script}" \
        "${csv_file}" \
        --output "${png_file}"
done

echo
echo "PID demonstrations completed successfully."
echo "CSV files and plots are in:"
echo "${output_dir}"
