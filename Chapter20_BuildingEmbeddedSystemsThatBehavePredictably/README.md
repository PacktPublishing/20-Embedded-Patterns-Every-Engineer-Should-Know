# Chapter 20: Building embedded systems that behave predictably

Four Linux programs communicate over localhost UDP: a simulated temperature sensor,
Weather Device, telemetry listener, and interactive command client. The device
reuses the Chapter 15 NMEA checksum protocol, Chapter 19 anomaly detectors, and
Chapter 7 exponential filter. No physical sensor is needed.

```sh
cmake -S Chapter20_BuildingEmbeddedSystemsThatBehavePredictably -B build/ch20
cmake --build build/ch20
cmake --install build/ch20
"$HOME/bin/run_ch20_tmux.sh"
```

Run these commands from the repository root. Installation puts all four binaries
and the launcher in `$HOME/bin`. The launcher shows the telemetry listener,
device, sensor, and command client in four panes of one tmux window. Click a
pane to focus it, use `Ctrl-b` followed by an arrow key to move the focus, and
use `Ctrl-b d` to detach. Running the launcher again restarts the four demo
programs in a fresh four-pane session. Quit each program
with `Ctrl-C`, and the command client with `quit`.

The command pane accepts `status`, `parameters`, `get ReportingInterval`,
`set ReportingInterval 1500`, `set TemperatureLimit 40`,
`set StaleTimeout 2500`, and `save`. `status`, `parameters`, and `get` send
their data to the **telemetry** pane. The listener keeps the latest requested
status and parameters at the top of its display while ordinary telemetry
continues to arrive below. The client waits up to one second for
`WXACK`, which only reports whether the command was accepted. If no ACK arrives,
the outcome is unknown. Commands can also be sent once from a shell, for example
`$HOME/bin/ch20_command_client status`.

For a deterministic run, restart the sensor pane with
`$HOME/bin/ch20_temperature_sensor --seed 42` (without random anomalies).
With `--random-anomalies`, seeded random events introduce range failures,
faults, sudden jumps, repeated readings, and occasional missing messages.
Stop the sensor with `Ctrl-C` for at least `StaleTimeout` milliseconds to
observe the `STALE` diagnostic; restart it to observe recovery.

At startup the device loads `$HOME/ch20_weather.state` if it contains a valid
versioned checksum-protected image. It then applies any command-line overrides:
`--reporting-ms`, `--temperature-limit`, and `--stale-ms`; use `--state-file`
to choose the file. `set` changes active values; `save` writes them to disk.
The launcher respects `CH20_STATE_FILE` as an optional state path. Defaults are
1000 ms reporting, 45 °C upper temperature bound, and 3000 ms stale timeout.
The lower temperature bound is −40 °C. Rate of change above 8 °C/s or five
identical readings are suspect. The filter uses α = 0.25 and only accepts good
samples. A bad reading does not replace the last filtered good value. Quality
and the reason remain visible in telemetry.

Sensor readings arrive on UDP 9520, commands on 9521, and telemetry on 9522,
all bound to loopback. Frames use Chapter 15 `$BODY*CHECKSUM` framing.
`WXSEN` is sensor input; `WXCMD` is a command; `WXACK` is a synchronous reply;
`WXTMP`, `WXDIA`, `WXSTS`, and `WXPAR` go to the independent listener. The
Weather Device's event loop checks timers even when no readings arrive.
