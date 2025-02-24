#!/usr/bin/env bash

set -e

# start the container and enter the shell
# https://stackoverflow.com/a/73613377
# 下列的选项是为了让 gdb 能够正常运行
# --cap-add=SYS_PTRACE --security-opt seccomp=unconfined
CONTAINER_NAME="compiler-dev"

if [ ! "$(docker ps -q -f name=${CONTAINER_NAME})" ]; then
    if [ "$(docker ps -aq -f status=exited -f name=${CONTAINER_NAME})" ]; then
        # start the existing container
        docker start ${CONTAINER_NAME}

        # exec into the running container
        docker exec -it ${CONTAINER_NAME} bash
    else
        # run a new container
        docker run --name ${CONTAINER_NAME} --cap-add=SYS_PTRACE --security-opt seccomp=unconfined -it -v .:/root/compiler maxxing/compiler-dev bash
    fi
else
    # exec into the running container
    docker exec -it ${CONTAINER_NAME} bash
fi
