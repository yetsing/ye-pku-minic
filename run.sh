#!/usr/bin/env bash

set -e

cmake -S "/root/compiler" -B "/tmp/debug" -DLIB_DIR="/opt/lib/native" -DINC_DIR="/opt/include"
cmake --build "/tmp/debug" --target compiler

/tmp/debug/compiler -koopa "$1" -o /tmp/hello.koopa
koopac /tmp/hello.koopa | llc --filetype=obj -o /tmp/hello.o
clang /tmp/hello.o -L$CDE_LIBRARY_PATH/native -lsysy -o /tmp/hello
/tmp/hello
