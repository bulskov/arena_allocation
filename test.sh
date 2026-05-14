#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Build directory '$BUILD_DIR' not found — run build.sh first." >&2
    exit 1
fi

ctest --test-dir "$BUILD_DIR" --output-on-failure --parallel "$(nproc)"
