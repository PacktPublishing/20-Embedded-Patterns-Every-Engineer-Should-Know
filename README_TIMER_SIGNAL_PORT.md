# Timer/signalfd additions for Chapter 3

This package adds two more readiness sources for the Chapter 3 event-loop example:

- `TimerFdSource`: wraps Linux `timerfd` for periodic and one-shot timers.
- `SignalFdSource`: wraps Linux `signalfd` so SIGINT/SIGTERM can be handled by `epoll` instead of an async signal handler.

The updated `nmea_monitor.cpp` demonstrates:

- UDP readiness
- UART/PTTY readiness
- periodic stats via `timerfd`
- optional duration timeout via one-shot `timerfd`
- clean shutdown via `signalfd`

No queues are introduced. This remains a Chapter 3 readiness-and-dispatch example.
