# Chapter 12 retry lab — first increment

This first increment establishes the normal BDS request/ACK path used by the
retry, timeout, and recovery lab.

It deliberately does **not** retry yet. The point is to prove the baseline path
before failure injection is added:

1. `ch12_retry_client` encodes `SetReportingIntervalRequest` as a BDS frame.
2. The request uses transaction ID 42 by default and sets the BDS idempotent flag.
3. `ch12_retry_server` receives and validates the frame through the Chapter 3
   `EpollReactor` and `UdpDatagramReceiver` machinery.
4. The server applies the requested interval and returns a BDS ACK carrying the
   same transaction ID.
5. The client validates the ACK and completes the transaction.

## Build

From this directory:

```bash
cmake -S . -B build
cmake --build build
cmake --install build
```

By default, the two executables and `run_retry_lab.sh` are installed in
`~/bin`.
```

The directory is expected to be beside `Common` in the book repository.

## Run

For a quick end-to-end test after installation:

```bash
~/bin/run_retry_lab.sh
```

The script starts the server, runs one client transaction, prints both sides of
the exchange, and cleans up the server.

To run the two sides manually, start the server:

```bash
~/bin/ch12_retry_server
```

In another terminal, run the client:

```bash
~/bin/ch12_retry_client
```

Typical server output:

```text
Chapter 12 retry server listening on UDP port 9100
Transaction 42: reporting interval set to 5000 ms, idempotent=yes
ACK 42 sent
```

Typical client output:

```text
Sending transaction 42: set reporting interval to 5000 ms
ACK 42 received
Transaction succeeded
```

Useful options:

```bash
./build/ch12_retry_server --port 9200
./build/ch12_retry_client --server 127.0.0.1 --port 9200 --interval-ms 2500 --transaction 43
```

The next increment adds a one-shot `timerfd`, bounded retry policy, and failure
injection while leaving the Chapter 3 reactor architecture unchanged.
