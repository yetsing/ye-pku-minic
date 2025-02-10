#!/usr/bin/env bash

set -e

# start the container and enter the shell
docker run -it --rm -v .:/root/compiler maxxing/compiler-dev bash
