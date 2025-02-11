#!/usr/bin/env bash

set -e

# docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
#     autotest -koopa -s lv1 /root/compiler

# docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
#     autotest -riscv -s lv1 /root/compiler

# docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
#     autotest -koopa -s lv3 /root/compiler

docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
    autotest -riscv -s lv3 /root/compiler
