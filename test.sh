#!/usr/bin/env bash

set -e

# if first argument is not empty, run all tests
if [ -n "$1" ]; then
    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -koopa -s lv1 /root/compiler

    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -riscv -s lv1 /root/compiler

    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -koopa -s lv3 /root/compiler

    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -riscv -s lv3 /root/compiler

    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -koopa -s lv4 /root/compiler

    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -riscv -s lv4 /root/compiler

    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -koopa -s lv5 /root/compiler

    docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
        autotest -riscv -s lv5 /root/compiler
fi

docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
    autotest -koopa -s lv6 /root/compiler

# docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
#     autotest -riscv -s lv6 /root/compiler
