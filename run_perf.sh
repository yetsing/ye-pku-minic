#!/usr/bin/env bash

set -e

cmake -S "/root/compiler" -B "/tmp/debug" -DLIB_DIR="/opt/lib/native" -DINC_DIR="/opt/include"
cmake --build "/tmp/debug" --target compiler
echo "Compilation successful: /tmp/debug/compiler"

/tmp/debug/compiler -perf "$1" -o /tmp/riscv_output.S
# 生成用来调试的目标文件
clang /tmp/riscv_output.S -c -o /tmp/hello.o -target riscv32-unknown-linux-elf -march=rv32im -mabi=ilp32
ld.lld /tmp/hello.o -L$CDE_LIBRARY_PATH/riscv32 -lsysy -o /tmp/hello
# qemu-riscv32-static -g 1234 /tmp/hello &
# cat /tmp/riscv_output.S
