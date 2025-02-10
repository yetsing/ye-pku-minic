#!/usr/bin/env bash

set -e

cmake -S "/root/compiler" -B "/tmp/debug" -DLIB_DIR="/opt/lib/native" -DINC_DIR="/opt/include"
cmake --build "/tmp/debug" --target compiler
echo "Compilation successful: /tmp/debug/compiler"

/tmp/debug/compiler "$@"
