#!/usr/bin/env bash

set -e

debug="${2:-false}"
if [ "$debug" = "false" ]; then
    cmake -S "/root/compiler" -B "/tmp/debug" -DLIB_DIR="/opt/lib/native" -DINC_DIR="/opt/include" -DENABLE_DEBUG_LOG=OFF
else
    cmake -S "/root/compiler" -B "/tmp/debug" -DLIB_DIR="/opt/lib/native" -DINC_DIR="/opt/include" -DENABLE_DEBUG_LOG=ON
fi

# cmake -S "/root/compiler" -B "/tmp/debug" -DLIB_DIR="/opt/lib/native" -DINC_DIR="/opt/include"
cmake --build "/tmp/debug" --target compiler
echo "Compilation successful: /tmp/debug/compiler"

/tmp/debug/compiler -koopa "$1" -o /tmp/koopa_output.txt
