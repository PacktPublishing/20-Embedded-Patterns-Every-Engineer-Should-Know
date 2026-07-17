#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

"""Plot the known physical signal and its simulated measurements."""

from __future__ import annotations

import argparse
import csv
import math
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt


@dataclass(frozen=True)
class Measurement:
    time_s: float
    true_value: float
    measured_value: float | None
    event: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot generated noisy measurements from a CSV file."
    )
    parser.add_argument("csv_file", type=Path, help="Input CSV file")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("noisy_measurements.png"),
        help="Output image path (default: noisy_measurements.png)",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Display the plot after saving it",
    )
    return parser.parse_args()


def read_measurements(path: Path) -> list[Measurement]:
    measurements: list[Measurement] = []

    with path.open(newline="", encoding="utf-8") as input_file:
        reader = csv.DictReader(input_file)
        expected = {"index", "time_s", "true_value", "measured_value", "event"}
        if reader.fieldnames is None or set(reader.fieldnames) != expected:
            raise ValueError(
                "CSV columns must be: index,time_s,true_value,measured_value,event"
            )

        for row_number, row in enumerate(reader, start=2):
            try:
                measured_text = row["measured_value"].strip()
                measured_value = float(measured_text) if measured_text else None
                measurements.append(
                    Measurement(
                        time_s=float(row["time_s"]),
                        true_value=float(row["true_value"]),
                        measured_value=measured_value,
                        event=row["event"].strip(),
                    )
                )
            except (TypeError, ValueError) as error:
                raise ValueError(f"invalid data on CSV row {row_number}") from error

    if not measurements:
        raise ValueError("CSV file contains no measurements")

    return measurements


def plot_measurements(measurements: list[Measurement], output: Path) -> None:
    times = [measurement.time_s for measurement in measurements]
    true_values = [measurement.true_value for measurement in measurements]
    measured_values = [
        measurement.measured_value
        if measurement.measured_value is not None
        else math.nan
        for measurement in measurements
    ]

    spike_times = [
        measurement.time_s
        for measurement in measurements
        if measurement.event == "spike" and measurement.measured_value is not None
    ]
    spike_values = [
        measurement.measured_value
        for measurement in measurements
        if measurement.event == "spike" and measurement.measured_value is not None
    ]
    missing_times = [
        measurement.time_s
        for measurement in measurements
        if measurement.event == "missing"
    ]

    figure, axes = plt.subplots(figsize=(10, 5.5), constrained_layout=True)
    axes.plot(times, true_values, linewidth=2.0, label="Underlying signal")
    axes.plot(
        times,
        measured_values,
        linewidth=0.9,
        marker=".",
        markersize=3,
        label="Measured value",
    )

    if spike_times:
        axes.scatter(
            spike_times,
            spike_values,
            marker="x",
            s=55,
            linewidths=1.5,
            label="Injected spike",
            zorder=3,
        )

    if missing_times:
        for index, missing_time in enumerate(missing_times):
            axes.axvline(
                missing_time,
                linestyle="--",
                linewidth=1.2,
                alpha=0.7,
                label="Missing measurement" if index == 0 else None,
        )

    axes.set_title("Simulated measurement artifacts")
    axes.set_xlabel("Time (seconds)")
    axes.set_ylabel("Value")
    axes.grid(True, alpha=0.25)
    axes.legend()

    output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output, dpi=160)


def main() -> int:
    args = parse_args()

    try:
        measurements = read_measurements(args.csv_file)
        plot_measurements(measurements, args.output)
    except (OSError, ValueError) as error:
        raise SystemExit(f"error: {error}") from error

    print(f"Wrote {args.output}")

    if args.show:
        plt.show()
    else:
        plt.close("all")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
