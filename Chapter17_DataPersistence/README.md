# Chapter 17: Data persistence and embedded storage strategies

This lab persists the parameter system from Chapter 16 in a bounded,
contiguous storage image. Each record contains a `ParameterID` followed by a
complete Version 1 BDS frame produced with the book's fixed-size
`MessageHeaderV1` and CRC-16 functions.

Place this directory beside `Common` and `Chapter16_Parameter_Management`.

## Build

```bash
cmake -S Chapter17_DataPersistence -B build/ch17 -DBUILD_TESTING=ON
cmake --build build/ch17 --parallel
ctest --test-dir build/ch17 --output-on-failure
cmake --install build/ch17
```

By default, installation places `ch17_weather_device`,
`ch17_image_inspector`, and `run_persistence_lab.sh` in `$HOME/bin`. Override
that location with `-DCH17_INSTALL_BINDIR=/another/path` when configuring.

## Create, inspect, and restore an image

```bash
build/ch17/ch17_weather_device \
    --image parameters.pimg --no-restore \
    --set-reporting-ms 2500 \
    --set-temperature-limit 55 \
    --set-operating-mode service \
    --save

build/ch17/ch17_image_inspector parameters.pimg
build/ch17/ch17_weather_device --image parameters.pimg
```

## Create a damaged copy

The inspection tool never damages its input file. It requires a separate
output path when injecting an error.

```bash
build/ch17/ch17_image_inspector parameters.pimg \
    --corrupt bad-payload --output corrupt.pimg

build/ch17/ch17_weather_device --image corrupt.pimg
```

Supported corruption modes are `bad-magic`, `bad-version`, `bad-header-crc`,
`truncate`, `bad-payload`, `unknown-parameter`, `duplicate-parameter`,
`type-mismatch`, and `invalid-value`.

## Demonstrate last-known-good preservation

```bash
build/ch17/ch17_weather_device \
    --image parameters.pimg \
    --set-reporting-ms 5000 \
    --save --simulate-write-failure

build/ch17/ch17_weather_device --image parameters.pimg
```

The first command exits with `save=WriteFailed`. The second command restores
the previous durable value because the simulated failure occurs before the
atomic `rename()` operation.
