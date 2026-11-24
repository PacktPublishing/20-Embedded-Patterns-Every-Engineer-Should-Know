#!/usr/bin/env bash

# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

set -euo pipefail

for scenario in reserved growing pool arena exhaustion
do
    memory_demo "${scenario}"
    echo
done
