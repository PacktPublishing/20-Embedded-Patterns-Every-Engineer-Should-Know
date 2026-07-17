# Measurement generator

`measurement_generator` creates a deterministic physical signal and then adds
simulated measurement artifacts: Gaussian noise, isolated spikes, and missing
measurements. The output CSV preserves the known underlying value so later
filtering examples can compare an estimate with the signal that produced it.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Generate sample data

```bash
./build/measurement_generator \
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
    --output measurements.csv
```

## Plot the measurements

```bash
python3 plot_measurements.py measurements.csv \
    --output noisy_measurements.png
```

The CSV columns are:

```text
index,time_s,true_value,measured_value,event
```

A missing measurement has an empty `measured_value` field. Its event is
`missing`; no numeric sentinel is used.

For the complete example, run:

```bash
./generate_example.sh
```
