#!/usr/bin/env bash

set -e

gdb --args /tmp/debug/compiler "-$1" "$2" -o "/tmp/unused_output.txt"
