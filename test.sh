#!/usr/bin/env bash

set -e

docker run -it --rm -v .:/root/compiler maxxing/compiler-dev \
    autotest -koopa -s lv1 /root/compiler
