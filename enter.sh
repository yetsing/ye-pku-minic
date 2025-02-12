#!/usr/bin/env bash

set -e

# start the container and enter the shell
# https://stackoverflow.com/a/73613377
# 下列的选项是为了让 gdb 能够正常运行
# --cap-add=SYS_PTRACE --security-opt seccomp=unconfined
docker run --cap-add=SYS_PTRACE --security-opt seccomp=unconfined -it --rm -v .:/root/compiler maxxing/compiler-dev bash
