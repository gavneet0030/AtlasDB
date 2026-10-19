#!/usr/bin/env bash
set -euo pipefail

cmake --preset linux-debug
cmake --build --preset debug --parallel
ctest --preset debug