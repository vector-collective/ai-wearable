#!/bin/sh
# Host-side logic test for the mic source-selection layer. No toolchain needed.
cd "$(dirname "$0")"
g++ -std=gnu++17 -Wall -Wextra -I . -I ../src test.cpp ../src/mic.cpp -o mic_test && ./mic_test
