#!/bin/bash
g++ common_utils_linux.cpp common_utils.cpp common_vaapi.cpp -c `pkg-config --cflags --libs libmfx` -g
gcc common_utils_linux.o common_utils.o common_vaapi.o simple_encode.c `pkg-config --cflags --libs libmfx` -g -o simple_encode
