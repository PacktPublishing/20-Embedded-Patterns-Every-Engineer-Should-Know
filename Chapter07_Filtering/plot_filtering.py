#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

"""Plot raw, validated, and filtered measurements from filter_measurements."""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
from typing import Iterable

import matplotlib.pyplot as plt


def parse_optional_float(text: str) -> float:
    return float(text) if text.strip() else math.nan


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot measurement_generator data and filtering results."
    )
    parser.add_argument("input", type=Path, help="Filtered CSV file")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("filtering_comparison.png"),
        help="Output image path (default: filtering_comparison.png)",
    )
    parser.add_argument(
        "--title",
        default="Measurement validation and filtering",
        help="Plot title",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Display the plot after saving it",
    )
    return parser.parse_args()


def require_columns(fieldnames: Iterable[str] | None) -> set[str]:
    required = {
        "time_s",
        "true_value",
        "measured_value",
        "validation_status",
        "moving_average",
        "exponential",
    }
    available = set(fieldnames or [])
    missing = required - available
    if missing:
        raise ValueError(
            "input CSV is missing required columns: "
            + ", ".join(sorted(missing))
        )
    return available


def main() -> int:
    arguments = parse_arguments()

    times: list[float] = []
    true_values: list[float] = []
    measured_values: list[float] = []
    moving_averages: list[float] = []
    exponential_values: list[float] = []
    rejected_times: list[float] = []
    rejected_values: list[float] = []
    missing_times: list[float] = []

    with arguments.input.open(newline="", encoding="utf-8") as input_file:
        reader = csv.DictReader(input_file)
        require_columns(reader.fieldnames)

        for row in reader:
            time_s = float(row["time_s"])
            measured = parse_optional_float(row["measured_value"])
            status = row["validation_status"]

            times.append(time_s)
            true_values.append(float(row["true_value"]))
            measured_values.append(measured)
            moving_averages.append(parse_optional_float(row["moving_average"]))
            exponential_values.append(parse_optional_float(row["exponential"]))

            if status == "missing":
                missing_times.append(time_s)
            elif status != "valid" and not math.isnan(measured):
                rejected_times.append(time_s)
                rejected_values.append(measured)

    figure, axes = plt.subplots(figsize=(12, 7))

    axes.plot(times, true_values, linewidth=2.0, label="Underlying signal")
    axes.scatter(
        times,
        measured_values,
        s=18,
        alpha=0.5,
        label="Raw measurement",
    )

    if any(not math.isnan(value) for value in moving_averages):
        axes.plot(
            times,
            moving_averages,
            linewidth=1.8,
            label="Moving average",
        )

    if any(not math.isnan(value) for value in exponential_values):
        axes.plot(
            times,
            exponential_values,
            linewidth=1.8,
            label="Exponential filter",
        )

    if rejected_times:
        axes.scatter(
            rejected_times,
            rejected_values,
            marker="x",
            s=70,
            linewidths=1.8,
            label="Rejected measurement",
        )

    for index, missing_time in enumerate(missing_times):
        axes.axvline(
            missing_time,
            linestyle="--",
            linewidth=1.2,
            alpha=0.7,
            label="Missing measurement" if index == 0 else None,
        )

    axes.set_title(arguments.title)
    axes.set_xlabel("Time (seconds)")
    axes.set_ylabel("Measurement")
    axes.grid(True, alpha=0.25)
    axes.legend()
    figure.tight_layout()

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(arguments.output, dpi=160)
    print(f"Wrote {arguments.output}")

    if arguments.show:
        plt.show()
    else:
        plt.close(figure)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
