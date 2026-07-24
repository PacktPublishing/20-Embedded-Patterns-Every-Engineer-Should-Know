# heap_monitor

`heap_monitor` is a small diagnostic library for measuring C++ heap activity
in an executable. Link it into a test or laboratory build to replace the
global `new` and `delete` operators.

The monitor records:

- allocation and deallocation counts
- requested bytes allocated and deallocated
- current and peak live requested bytes
- allocation count and requested bytes after initialization

Mark the beginning of steady-state operation after the system has initialized:

```cpp
auto system = initialize_system(configuration);

heap_monitor::begin_steady_state();
system.run();
```

Read the counters without causing another allocation:

```cpp
const auto result = heap_monitor::statistics();
```

The statistics cover allocations made through the replaceable global C++
allocation functions in the complete executable, including C++ libraries that
use them. They do not include direct calls to `malloc`, allocator bookkeeping,
thread stacks, memory-mapped regions, or the process resident set size.

The monitor changes allocation timing slightly and is intended for memory-use
experiments, not precise allocator-latency measurements.
