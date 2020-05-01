#!/bin/bash
clear
BUILD_DIR=./build
COMPILATION_UNITS=./main.c
EX_NAME=chip8
C_VERSION=-std=c99

if [ ! -d ./build ]; then
    echo "Creating $BUILD_DIR"
    mkdir $BUILD_DIR
fi

echo "Building..."
#
gcc -g $COMPILATION_UNITS $C_VERSION -Wall -Wextra -Wno-missing-braces -Wno-unused-function -lm -lSDL2 -lGL -o "$BUILD_DIR"/"$EX_NAME"
EC=$?

[ $EC -eq 0 ] && echo "Build succesfull" || echo "Build failed"
