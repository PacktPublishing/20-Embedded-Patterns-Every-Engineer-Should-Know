#!/usr/bin/env python3

import csv
from pathlib import Path

import matplotlib.pyplot as plt


INPUT_PATH = Path("output/process_oven.csv")
OUTPUT_PATH = Path("output/process_oven.png")

HEATERS = {
    "preheat": "Preheat",
    "upper_radiant": "Upper radiant",
    "lower_radiant": "Lower radiant",
    "left_wall": "Left wall",
    "right_wall": "Right wall",
    "core": "Core",
    "cure": "Cure",
    "exhaust_trim": "Exhaust trim",
}


def read_results():
    with INPUT_PATH.open(newline="", encoding="utf-8") as input_file:
        reader = csv.DictReader(input_file)

        required_columns = {"elapsed_ms", "active_rampers", *HEATERS}
        missing_columns = required_columns - set(reader.fieldnames or [])

        if missing_columns:
            missing = ", ".join(sorted(missing_columns))
            raise ValueError(f"CSV is missing required columns: {missing}")

        rows = list(reader)

    if not rows:
        raise ValueError("CSV contains no process-oven results")

    elapsed_seconds = [float(row["elapsed_ms"]) / 1000.0 for row in rows]
    active_rampers = [int(row["active_rampers"]) for row in rows]
    commands = {
        column: [float(row[column]) for row in rows]
        for column in HEATERS
    }

    return elapsed_seconds, active_rampers, commands


def main():
    try:
        elapsed_seconds, active_rampers, commands = read_results()
    except (OSError, ValueError) as error:
        raise SystemExit(f"Unable to plot {INPUT_PATH}: {error}") from error

    figure, (command_axes, active_axes) = plt.subplots(
        2,
        1,
        figsize=(10, 7),
        sharex=True,
        gridspec_kw={"height_ratios": (4, 1)},
    )

    for column, label in HEATERS.items():
        command_axes.plot(
            elapsed_seconds,
            commands[column],
            marker="o",
            linewidth=1.8,
            markersize=4,
            label=label,
        )

    command_axes.set_title("Process oven heater transitions")
    command_axes.set_ylabel("Commanded power (%)")
    command_axes.set_ylim(0, 100)
    command_axes.grid(True, alpha=0.3)
    command_axes.legend(
        loc="center left",
        bbox_to_anchor=(1.02, 0.5),
        frameon=False,
    )

    active_axes.step(
        elapsed_seconds,
        active_rampers,
        where="post",
        linewidth=2,
        color="tab:gray",
    )
    active_axes.scatter(elapsed_seconds, active_rampers, color="tab:gray", s=18)
    active_axes.set_xlabel("Nominal elapsed time (s)")
    active_axes.set_ylabel("Active\nrampers")
    active_axes.set_yticks(range(0, 9, 2))
    active_axes.set_ylim(-0.5, 8.5)
    active_axes.grid(True, alpha=0.3)

    figure.tight_layout()
    figure.savefig(OUTPUT_PATH, dpi=150, bbox_inches="tight")
    plt.close(figure)

    print(f"Wrote process-oven plot to {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
