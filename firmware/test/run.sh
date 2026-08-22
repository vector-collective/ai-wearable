#!/bin/sh
# Host-side logic tests. No ESP toolchain needed.
cd "$(dirname "$0")"
set -e
g++ -std=gnu++17 -Wall -Wextra -I . -I ../src test.cpp ../src/mic.cpp -o mic_test && ./mic_test
g++ -std=gnu++17 -Wall -Wextra -I . -I ../src test_ui.cpp -o ui_test && ./ui_test
