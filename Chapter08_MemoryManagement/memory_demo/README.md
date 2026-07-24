# Memory management demo

This Chapter 8 laboratory application compares five controlled memory
behaviors:

- a vector whose capacity is reserved during initialization
- a vector that grows during steady-state operation
- a bounded PMR pool backed by a fixed buffer
- a bounded PMR arena reset at a shared lifetime boundary
- deliberate exhaustion of a bounded PMR arena

Each scenario runs as a separate process because `heap_monitor` records
executable-wide cumulative statistics. The program prints initialization,
steady-state, and overall heap statistics and checks whether the observed
behavior matches the scenario's expectation.

## Prerequisite

Build and install `Tools/heap_monitor` before building this directory by
itself. When building from the root of the book repository, CMake uses the
`heap_monitor` target directly and a separate installation is unnecessary.

## Building and installing

From this directory:

```bash
cmake -S . -B build
cmake --build build
sudo cmake --install build
```

The default install places `memory_demo` and `run_memory_demo.sh` in
`/usr/local/bin`. Use `-DCMAKE_INSTALL_PREFIX="$HOME/.local"` during
configuration for a user-only installation.

## Running the laboratory

Run every scenario:

```bash
run_memory_demo.sh
```

Or run one scenario:

```bash
memory_demo reserved
memory_demo growing
memory_demo pool
memory_demo arena
memory_demo exhaustion
```

The `growing` scenario reports `PASS` when it detects the allocations that
were intentionally introduced. The other scenarios report `PASS` only when
the expected bounded behavior is observed.
