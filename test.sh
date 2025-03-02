#!/usr/bin/env bash

set -e

if [ -n "$1" ]; then
    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -koopa -s "$1" /root/compiler

    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -riscv -s "$1" /root/compiler
else
    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -koopa /root/compiler

    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -riscv /root/compiler
fi
