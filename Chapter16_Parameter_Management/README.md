# Chapter 16 parameter-management lab

This lab implements the Chapter 16 parameter-management architecture as one
small host-first program:

- strongly typed parameter traits
- fixed `ParameterID` enumeration and `constexpr` handler registry
- compile-time traits bridged to runtime requests through type-erased function pointers
- fixed BDS-encoded in-memory parameter image
- provenance and deterministic source precedence
- validated, transaction-like updates
- state-change notifications
- one-owner threading with bounded request and notification queues

The lab expects the book's `Common` directory to be beside this directory and
uses `CommonBds` plus `BoundedQueue` from that shared code.

## Build, test, and install

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

By default, `cmake --install build` installs these files in `$HOME/bin`:

- `ch16_parameter_lab`
- `run_parameter_lab.sh`

Run the installed lab with:

```bash
run_parameter_lab.sh
```

No `tmux` session is required. The demonstration is a single process that
creates its producer, parameter-owner, and notification-consumer threads
internally.

## What the scripted run demonstrates

The request producer submits a fixed sequence of updates. The sequence shows:

1. a persistent value replacing the compiled default;
2. a configuration-file value replacing the persisted value;
3. a command-line value replacing the configuration-file value;
4. a lower-precedence configuration-file update being rejected;
5. a runtime reporting-interval update being accepted;
6. an out-of-range reporting interval being rejected;
7. a runtime change to a startup-only parameter being rejected; and
8. a runtime operating-mode update being accepted.

Only accepted updates increment the generation counter and create a
`ParameterChangeEvent`. The notification consumer reads the latest typed value
from the parameter store rather than carrying another copy in the event.

## Parameter image layout

The image is deliberately small and fixed:

| Parameter | Offset | Encoded size |
| --- | ---: | ---: |
| `ReportingInterval` | 0 | 4 bytes |
| `TemperatureLimit` | 4 | 4 bytes |
| `OperatingMode` | 8 | 1 byte |

Total: 9 bytes.

The image uses little-endian BDS encoding. Runtime provenance is stored
separately in a fixed array indexed by `ParameterID`.
