# Process oven ramping lab

This host-first lab simulates the coordinated transition of eight heater-power
commands. `Ramper` represents one complete scalar transition. `RampDriver`
advances all active rampers once per cycle and removes each completed
transition from the active field.

Build and run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/process_oven
```

The default run advances simulated time without waiting and writes
`process_oven.csv`. To pace the same deterministic steps using the configured
wall-clock period, run:

```bash
./build/process_oven --realtime
```
