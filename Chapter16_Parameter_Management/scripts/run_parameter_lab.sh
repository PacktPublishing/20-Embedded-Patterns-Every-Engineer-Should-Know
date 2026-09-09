#!/usr/bin/env bash
set -euo pipefail

BIN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${BIN_DIR}/ch16_parameter_lab" "$@"
