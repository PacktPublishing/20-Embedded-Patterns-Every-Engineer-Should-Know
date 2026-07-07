# Chapter 3 NMEA monitor

Small readiness-based receiver demo for Chapter 3.

It registers UDP, UART, `timerfd`, and `signalfd` sources with `EpollReactor`, dispatches readiness events, and prints received ASCII NMEA-style sentences.

No queues are used. This example is intentionally about readiness and direct dispatch only.

## Common files to add

Copy these into `Common`:

```sh
cp Common/include/TimerFdSource.h <repo>/Common/include/
cp Common/include/SignalFdSource.h <repo>/Common/include/
cp Common/src/TimerFdSource.cpp <repo>/Common/src/
cp Common/src/SignalFdSource.cpp <repo>/Common/src/
```

Then add these source files to the Common CMake target:

```cmake
src/TimerFdSource.cpp
src/SignalFdSource.cpp
```

## UDP example

Terminal 1:

```sh
./ch03_nmea_monitor --udp-port 9000
```

Terminal 2:

```sh
nmea_sim --udp 127.0.0.1:9000 --temp-hz 2 --pressure-hz 1
```

## UART/PTTY example

Terminal 1:

```sh
socat -d -d pty,raw,echo=0 pty,raw,echo=0
```

`socat` prints two PTY devices, for example `/dev/pts/2` and `/dev/pts/3`.

Terminal 2:

```sh
./ch03_nmea_monitor --uart /dev/pts/2
```

Terminal 3:

```sh
nmea_sim --uart /dev/pts/3 --temp-hz 2
```

## Timer and signal examples

Print stats every second and stop after 10 seconds:

```sh
./ch03_nmea_monitor --udp-port 9000 --stats-every 1 --duration 10
```

Stop with Ctrl-C. SIGINT and SIGTERM are handled through `signalfd`, so shutdown is dispatched by the same `epoll` loop as UDP, UART, and timers.
