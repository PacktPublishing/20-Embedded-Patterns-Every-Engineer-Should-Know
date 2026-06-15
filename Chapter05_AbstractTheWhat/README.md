# Chapter 5 Imaging Capability Interfaces

This is a cleaned-up teaching subset derived from an older imaging framework idea.
It is intentionally not a full framework. It exists to support Chapter 5,
"Abstract the WHAT, Not the HOW."

Key points demonstrated:

- `IImageSource` produces `ImageBlock`, which represents image data, not a full image object.
- `ImageBlock` is a lightweight value-semantics wrapper around shared image data.
- `ImageBlock` uses `std::shared_ptr<const std::byte>` internally so copies are shallow and safe.
- `clone()` performs an explicit deep copy when independent storage is needed.
- `PixelFormat`, `strideBytes()`, row access, and `std::span` make image data layout explicit.
- `fromExternal()` supports externally owned buffers such as DMA memory, frame-grabber ring buffers, simulators, and vendor APIs.
- Capability interfaces describe what a component can do.
- Capability metadata describes valid operating space: ranges, discrete choices, and modes.
- Algorithms depend on the capabilities they require, not on concrete devices.
- Runtime `supports()` checks are not required for the closed-world embedded case; composition wires the known implementation to the algorithm.

Files of interest:

- `IImageSource.h`
- `ImageBlock.h`
- `IIntegration.h`
- `Range.h`
- `AdjustIntegration.h`
- `Defaults.h`
- `IAreaOfInterest.h`
- `IBinning.h`
- `IGain.h`
- `IOffset.h`

Build the example:

```sh
cmake -S . -B build
cmake --build build
./build/chapter5_imaging_example
```

The example requires C++20 for `std::span`.
