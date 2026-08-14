# Chapter 12 retry, timeout, and recovery lab

This lab builds a bounded BDS request/response transaction over UDP using the
readiness-based infrastructure introduced in Chapter 3 and the framing support
introduced in Chapter 11.

The client sends one `SetReportingIntervalRequest` transaction at a time. The
request uses the BDS transaction identifier and idempotent message flag. A
one-shot `TimerFdSource` bounds the response wait and also schedules retry
delays. The server can deliberately drop acknowledgments, return a retryable
NACK, remain silent, and recognize a retried transaction that it already
completed.

The default client policy is:

```text
response timeout: 500 ms
retry delay:      250 ms
maximum attempts: 3
```

`maximum attempts` includes the original request. Three attempts therefore mean
the original request plus at most two retries.

## Build and install

From this directory:

```bash
cmake -S . -B build
cmake --build build
cmake --install build
```

By default, the executables and runner are installed in `~/bin`:

```text
ch12_retry_client
ch12_retry_server
run_retry_lab.sh
```

## Run all scenarios

```bash
run_retry_lab.sh
```

The runner demonstrates five cases:

1. Normal request/ACK.
2. Lost ACK followed by timeout, retry, duplicate detection, and successful ACK.
3. Retryable `Busy` NACK followed by a successful retry.
4. Non-retryable `InvalidValue` NACK.
5. Silence through all permitted attempts, followed by recovery that marks the
   remote service unavailable.

## Run manually

Start the server in one terminal:

```bash
ch12_retry_server --mode drop-first-ack
```

Then run the client in another:

```bash
ch12_retry_client
```

Server modes are:

```text
normal
drop-first-ack
busy-once
silent
```

Useful client options include:

```text
--timeout-ms <ms>
--retry-delay-ms <ms>
--max-attempts <count>
--interval-ms <ms>
--transaction <id>
```

The server accepts reporting intervals from 100 through 60000 milliseconds.
Values outside that range produce a non-retryable `InvalidValue` NACK.
