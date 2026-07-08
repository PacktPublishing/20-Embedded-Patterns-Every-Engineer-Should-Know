# Chapter 3 – Event loops and readiness-based dispatch

This chapter demonstrates readiness-based dispatch using Linux `epoll`. The main example is a small host-first demo that receives simulated NMEA-style messages from both UDP and UART-style inputs, while also handling timer and signal events through the same event loop.

The goal is not to build the full Weather Reporter Device yet. The goal is to prove the ingestion model:

```text
external input becomes ready
    -> epoll reports readiness
    -> source-specific handler reads input
    -> received NMEA-style sentence is printed
```

No queues are used in this chapter. Queue boundaries, producer/consumer behavior, and backpressure are introduced in Chapter 4.

## Contents

```text
Chapter03_EventLoops/
  CMakeLists.txt
  README.md
  ch03_readiness_demo.sh
  run_nmea_sources.sh

  readiness_demo/
    nmea_monitor.cpp
    README.md
```

## Programs and scripts

### `ch03_nmea_monitor`

`ch03_nmea_monitor` is the Chapter 3 readiness demo executable. It can listen for:

- UDP datagrams
- UART or pseudo-terminal input
- periodic `timerfd` events
- shutdown signals through `signalfd`

It prints received NMEA-style sentences and periodic source statistics.

### `run_nmea_sources.sh`

`run_nmea_sources.sh` starts two `nmea_sim` instances:

- one sending simulated NMEA messages over UDP
- one sending simulated NMEA messages to a UART/PTTY path

This is a convenience script used by the full demo.

### `ch03_readiness_demo.sh`

`ch03_readiness_demo.sh` creates a three-pane `tmux` demo:

```text
top pane:    socat creates a UART/PTTY pair
middle pane: nmea_sim sends UDP and UART messages
bottom pane: ch03_nmea_monitor receives both sources
```

The script parses the PTY paths printed by `socat`, starts the monitor first, then starts the simulated sources. This avoids the common mistake of using the terminal pane’s own `tty` path as the UART device.

## Prerequisites

The book’s Vagrant environment should already provide the required host tools, including:

- `tmux`
- `socat`
- CMake
- a C++20-capable compiler

The demo also expects these programs to be available on `PATH`:

```text
nmea_sim
ch03_nmea_monitor
run_nmea_sources.sh
ch03_readiness_demo.sh
```

## Build and install `nmea_sim`

`nmea_sim` lives outside this chapter because it is reused across the book.

From the `nmea_sim` project directory:

```sh
cmake -S . -B build
cmake --build build
cmake --install build
```

This installs:

```text
$HOME/bin/nmea_sim
```

Make sure `$HOME/bin` is on your `PATH`.

## Build and install the Chapter 3 demo

From `Chapter03_EventLoops`:

```sh
cmake -S . -B build
cmake --build build
cmake --install build
```

This installs:

```text
$HOME/bin/ch03_nmea_monitor
$HOME/bin/run_nmea_sources.sh
$HOME/bin/ch03_readiness_demo.sh
```

## Run the full readiness demo

The easiest way to run the demo is:

```sh
ch03_readiness_demo.sh
```

The script creates the `tmux` layout, starts `socat`, discovers the PTY pair, starts `ch03_nmea_monitor`, and then starts the simulated NMEA sources.

The bottom pane should show output similar to:

```text
Listening on UDP port 9000
Listening on UART /dev/pts/7 @ 115200 baud
Stats timer every 5 second(s)
Waiting for NMEA input. Press Ctrl-C to stop.

[udp 127.0.0.1:60392] $PBOOK,TEMP,22.00,C*14
[udp 127.0.0.1:60392] $PBOOK,TEMP,22.01,C*15
[uart] $PBOOK,TEMP,22.00,C*14

--- stats ---
udp:  frames: 2  bytes: 48  dropped: 0  read err: 0  overflow: 0
uart: bytes=24 frames=1 dropped=0 read_errors=0 overflow_errors=0
stats_timer: expirations=3 read_errors=0
signal_fd: signals=0 read_errors=0
```

The exact PTY paths and UDP source port will vary.

## Optional screenshot delay

By default, `ch03_readiness_demo.sh` waits briefly before starting the simulated sources. This gives you time to arrange the terminal or prepare a screenshot.

To change the delay:

```sh
START_DELAY=3 ch03_readiness_demo.sh
```

To start almost immediately:

```sh
START_DELAY=1 ch03_readiness_demo.sh
```

## Manual UDP demo

Run the monitor:

```sh
ch03_nmea_monitor --udp-port 9000 --stats-every 5
```

In another terminal, run:

```sh
nmea_sim --udp 127.0.0.1:9000 --temp-hz 0.5 --pressure-hz 0.2
```

## Manual UART/PTTY demo

Create a pseudo-terminal pair:

```sh
socat -d -d pty,raw,echo=0 pty,raw,echo=0
```

`socat` prints two PTY paths, for example:

```text
PTY is /dev/pts/7
PTY is /dev/pts/8
```

Run the monitor on one side:

```sh
ch03_nmea_monitor --uart /dev/pts/7 --stats-every 5
```

Run the simulator on the other side:

```sh
nmea_sim --uart /dev/pts/8 --temp-hz 0.25
```

Use the two PTY paths printed by `socat`. Do not use the terminal pane’s own `tty` path as the UART device. Opening the current terminal as the UART device can put the pane into serial-style mode and make the terminal behave strangely.

## Manual UDP and UART demo

Create the PTY pair:

```sh
socat -d -d pty,raw,echo=0 pty,raw,echo=0
```

Suppose it prints:

```text
PTY is /dev/pts/7
PTY is /dev/pts/8
```

Start the monitor:

```sh
ch03_nmea_monitor --udp-port 9000 --uart /dev/pts/7 --stats-every 5
```

Start the sources:

```sh
run_nmea_sources.sh /dev/pts/8
```

The monitor should show both UDP and UART messages.

## What this demo shows

This example demonstrates that different event sources can be coordinated through one readiness-based loop:

```text
UDP socket ready
    -> epoll wakes
    -> UdpDatagramReceiver reads one datagram

UART/PTTY ready
    -> epoll wakes
    -> UartLineReceiver reads available bytes and emits complete lines

timerfd ready
    -> epoll wakes
    -> periodic statistics are printed

signalfd ready
    -> epoll wakes
    -> shutdown is handled cleanly
```

This is the Chapter 3 boundary. The event loop detects readiness and dispatches to the appropriate source handler. Later chapters add queues, backpressure, filtering, control logic, and more complete processing pipelines.
