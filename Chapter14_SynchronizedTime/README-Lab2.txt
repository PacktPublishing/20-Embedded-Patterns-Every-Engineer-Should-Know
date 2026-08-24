Chapter 14 Lab 2 - synchronized remote/local measurement fusion

Copy these files into Chapter14_SynchronizedTime:

  SynchronizedMeasurementProtocol.h
  wind_speed_tx.cpp
  synchronized_fusion.cpp
  run_measurement_lab.sh
  CMakeLists.txt

This CMakeLists preserves ch14_clock_monitor if clock_monitor.cpp is present.
It expects the Chapter 12 CommonBds additions, including MessageFrame.h.

Run:

  chmod +x run_measurement_lab.sh
  ./run_measurement_lab.sh

The script is independent of Lab 1. It starts both VMs, starts PTP silently,
waits for Dev to enter PTP SLAVE state, then opens a two-pane tmux display.

Chronos:
  - wind-speed measurement every 100 ms
  - cadence scheduled with steady_clock
  - event timestamp captured with PTP-disciplined system_clock
  - BDS frame over UDP to 192.168.56.15:45014

Dev:
  - receiver thread decodes remote WindSpeed into a bounded queue
  - local acquisition thread creates WindDirection every 333 ms
  - second bounded queue carries local measurements
  - fusion thread keeps newest remote speed and evaluates each local direction
  - 40 ms event-time match window
  - std::variant<WindSpeed, WindDirection> is the queue value type

Commands:

  ./run_measurement_lab.sh stop
      Stop demo/PTP, leave VMs running.

  ./run_measurement_lab.sh halt
      Stop demo/PTP and vagrant halt both VMs.

  ./run_measurement_lab.sh attach
      Reattach to the tmux session.
