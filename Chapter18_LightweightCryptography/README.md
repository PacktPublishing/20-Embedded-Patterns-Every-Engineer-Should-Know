# Chapter 18 labs

This project contains the labs for Chapter 18, *Lightweight cryptography*.

## Prerequisites

The supplied Vagrant environment installs the header-only Ascon-AEAD128
implementation and its `subtle` dependency under `/usr/local/include`.
The labs require C++20.

## Build both labs

From the `Chapter18Labs` directory, run:

```bash
cmake -S . -B build -G Ninja
cmake --build build
cmake --install build
```

The executables are installed in `$HOME/bin`; administrator privileges are not
required.

The top-level project builds each Chapter 18 lab. Lab 2 will be included
automatically when its `ch18_weather_device/CMakeLists.txt` is added.

## Lab 1: authenticated-encryption fundamentals

Run:

```bash
ch18_ascon_basics
```

The program:

1. Encrypts a harmless string using Ascon-AEAD128.
2. Displays the nonce, associated data, ciphertext, and authentication tag.
3. Authenticates and decrypts the ciphertext.
4. Flips one bit in the ciphertext.
5. Demonstrates that authentication fails and the plaintext is rejected.

The fixed key and nonce are for this repeatable demonstration only. Production
systems must protect secret keys and must not reuse a nonce for two encryptions
performed with the same key.
