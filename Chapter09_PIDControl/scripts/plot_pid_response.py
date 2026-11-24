#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt


def read_csv(path: Path) -> dict[str, list[float]]:
    with path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        data = {name: [] for name in reader.fieldnames or []}
        for row in reader:
            for name, value in row.items():
                data[name].append(float(value))
    return data


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot a smoker PID simulation.")
    parser.add_argument("csv_file", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    args = parser.parse_args()

    data = read_csv(args.csv_file)
    figure, (temperature_axis, command_axis, terms_axis) = plt.subplots(
        3, 1, figsize=(10, 9), sharex=True, constrained_layout=True
    )

    time = data["time_s"]
    temperature_axis.plot(time, data["setpoint_f"], "--", label="Setpoint")
    temperature_axis.plot(time, data["temperature_f"], label="Temperature")
    temperature_axis.set_ylabel("Temperature (°F)")
    temperature_axis.grid(alpha=0.3)
    temperature_axis.legend()

    command_axis.plot(time, data["fan_command"], color="tab:orange", label="Fan")
    command_axis.plot(time, data["raw_output"], ":", color="tab:red", label="Raw output")
    command_axis.set_ylabel("Fan command")
    command_axis.set_ylim(-0.1, max(1.1, max(data["raw_output"]) * 1.05))
    command_axis.grid(alpha=0.3)
    command_axis.legend()

    terms_axis.plot(time, data["p_term"], label="P")
    terms_axis.plot(time, data["i_term"], label="I")
    terms_axis.plot(time, data["d_term"], label="D")
    terms_axis.set_xlabel("Time (s)")
    terms_axis.set_ylabel("PID contribution")
    terms_axis.grid(alpha=0.3)
    terms_axis.legend()

    disturbance_times = [
        t for t, active in zip(time, data["disturbance"]) if active != 0.0
    ]
    for axis in (temperature_axis, command_axis, terms_axis):
        for disturbance_time in disturbance_times:
            axis.axvline(disturbance_time, color="black", alpha=0.45, linestyle="--")

    figure.suptitle(args.csv_file.stem.replace("_", " ").title())
    output = args.output or args.csv_file.with_suffix(".png")
    figure.savefig(output, dpi=150)
    print(f"Wrote {output}")


if __name__ == "__main__":
    main()
