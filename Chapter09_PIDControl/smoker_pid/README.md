# Smoker PID lab

This host-first lab models a BBQ smoker and controls it with a discrete PID
controller. The plant and controller are independent components owned by one
simulation runner.

## Build and test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

```text
Usage: ./build/smoker_sim open-loop|p|pi|pid|windup|disturbance [output.csv]
```

For example:

```bash
./build/smoker_sim pid output/pid.csv
python3 scripts/plot_pid_response.py output/pid.csv
```

Run every scenario, test the code, and generate every plot with:

```bash
./scripts/run_pid_demo.sh
```

The scenarios demonstrate:

- `open-loop`: a fixed fan command with no feedback
- `p`: proportional control and steady-state error
- `pi`: integral correction
- `pid`: proportional, integral, and derivative control
- `windup`: saturation with anti-windup deliberately disabled
- `disturbance`: a 55°F temperature drop at 500 seconds, representing an
  opened smoker lid

The CSV output exposes the temperature, error, individual PID contributions,
raw output, limited fan command, saturation state, and disturbances.
