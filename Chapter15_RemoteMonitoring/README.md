# Chapter 15 - Remote Monitoring and Control

Build, test, and install:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

CMake installs the lab executables and run script in `$HOME/bin`:

- `ch15_weather_device`
- `ch15_remote_monitor`
- `run_monitoring_lab.sh`

Run the lab:

```bash
run_monitoring_lab.sh
```

The script opens a tmux session with the Weather Device in the left pane and the Remote Monitor in the right pane. The monitor subscribes to temperature, pressure, health, and diagnostic reports, then unsubscribes from temperature after ten seconds while the remaining reports continue.
