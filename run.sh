#!/usr/bin/env bash

set -e

cmake -S "/root/compiler" -B "/tmp/debug" -DLIB_DIR="/opt/lib/native" -DINC_DIR="/opt/include"
cmake --build "/tmp/debug" --target compiler

/tmp/debug/compiler -riscv "$1" -o /tmp/hello.S
clang /tmp/hello.S -c -o /tmp/hello.o -target riscv32-unknown-linux-elf -march=rv32im -mabi=ilp32
ld.lld /tmp/hello.o -L$CDE_LIBRARY_PATH/riscv32 -lsysy -o /tmp/hello
qemu-riscv32-static /tmp/hello
