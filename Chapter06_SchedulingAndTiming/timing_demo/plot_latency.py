#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

import argparse
import csv
import math
import os
import statistics

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt


def percentile(values, fraction):
    if not values:
        return 0.0

    ordered = sorted(values)
    index = fraction * (len(ordered) - 1)
    lower = math.floor(index)
    upper = math.ceil(index)

    if lower == upper:
        return float(ordered[lower])

    weight = index - lower
    return float(ordered[lower]) * (1.0 - weight) + float(ordered[upper]) * weight


def infer_period_us(rows):
    scheduled = [int(row["scheduled_ns"]) for row in rows]

    if len(scheduled) < 2:
        return None

    deltas = [
        scheduled[index] - scheduled[index - 1]
        for index in range(1, len(scheduled))
    ]

    return statistics.median(deltas) / 1000.0


def read_rows(path):
    with open(path, "r", newline="") as input_file:
        return list(csv.DictReader(input_file))


def main():
    parser = argparse.ArgumentParser(
        description="Plot latency_probe CSV output as a PNG file."
    )

    parser.add_argument("input_csv", help="Input CSV file from latency_probe")
    parser.add_argument("output_png", help="Output PNG path")
    parser.add_argument(
        "--period-us",
        type=float,
        default=None,
        help="Period/deadline in microseconds. If omitted, infer from scheduled_ns.",
    )
    parser.add_argument(
        "--title",
        default=None,
        help="Optional plot title",
    )

    args = parser.parse_args()

    rows = read_rows(args.input_csv)

    if not rows:
        raise RuntimeError(f"No rows found in {args.input_csv}")

    sample_indices = [int(row["sample_index"]) for row in rows]
    total_latency_us = [float(row["total_latency_us"]) for row in rows]
    wake_latency_us = [float(row["wake_latency_us"]) for row in rows]

    period_us = args.period_us
    if period_us is None:
        period_us = infer_period_us(rows)

    overrun_count = sum(1 for row in rows if int(row["overrun"]) != 0)

    minimum = min(total_latency_us)
    average = statistics.mean(total_latency_us)
    p95 = percentile(total_latency_us, 0.95)
    p99 = percentile(total_latency_us, 0.99)
    maximum = max(total_latency_us)

    os.makedirs(os.path.dirname(os.path.abspath(args.output_png)), exist_ok=True)

    plt.figure(figsize=(10, 5))
    plt.plot(sample_indices, total_latency_us, label="Total latency")
    plt.plot(sample_indices, wake_latency_us, label="Wake latency")

    if period_us is not None:
        plt.axhline(period_us, linestyle="--", label="Period/deadline")

    title = args.title
    if title is None:
        title = os.path.basename(args.input_csv)

    plt.title(title)
    plt.xlabel("Sample index")
    plt.ylabel("Latency (us)")
    plt.legend(loc="upper right")

    summary = (
        f"min={minimum:.1f} us\n"
        f"avg={average:.1f} us\n"
        f"p95={p95:.1f} us\n"
        f"p99={p99:.1f} us\n"
        f"max={maximum:.1f} us\n"
        f"overruns={overrun_count}"
    )

    plt.text(
        0.01,
        0.98,
        summary,
        transform=plt.gca().transAxes,
        va="top",
        bbox={"facecolor": "white", "alpha": 0.8},
    )

    plt.tight_layout()
    plt.savefig(args.output_png, dpi=150)

    print(f"Wrote {args.output_png}")


if __name__ == "__main__":
    main()
