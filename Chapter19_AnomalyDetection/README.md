# Chapter 19 lab: Lightweight anomaly detection at the edge

This lab builds a small, typed processing pipeline:

```text
Measurement source -> Anomaly detection -> Result display
```

The anomaly node combines inclusive range, rate-of-change, stuck-value,
and stale-reading checks. Measurements are always forwarded with a quality
classification of `Good`, `Suspect`, or `Invalid`.

## Build and run

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
ch19_weather_device
```

The install step places `ch19_weather_device` in `$HOME/bin`.
