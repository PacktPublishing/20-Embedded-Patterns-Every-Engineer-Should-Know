Chapter 14 Lab 2 - synchronized temperature and pressure measurements

Lab 2 demonstrates measurement synchronization using timestamps from two
PTP-synchronized computers.

Chronos:
  - simulates a remote temperature measurement every 100 ms
  - schedules acquisition with steady_clock
  - stamps each measurement with PTP-disciplined system_clock
  - sends the framed measurement over UDP to Dev

Dev:
  - receives remote Temperature measurements
  - generates local Pressure measurements every 333 ms
  - compares acquisition timestamps using a 40 ms match window
  - reports MATCH or MISS

Build and install before running the lab:

  cmake -S . -B build
  cmake --build build -j
  cmake --install build

Then run:

  run_measurement_lab.sh

Commands:

  run_measurement_lab.sh stop
      Stop demo/PTP, leave VMs running.

  run_measurement_lab.sh halt
      Stop demo/PTP and vagrant halt both VMs.

  run_measurement_lab.sh attach
      Reattach to the tmux session.
