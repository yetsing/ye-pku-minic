#!/usr/bin/env bash

set -e

cmake -S "/root/compiler" -B "/tmp/debug" -DLIB_DIR="/opt/lib/native" -DINC_DIR="/opt/include"
cmake --build "/tmp/debug" --target compiler
gdb --args /tmp/debug/compiler "-$1" "$2" -o "/tmp/unused_output.txt"
