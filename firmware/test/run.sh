#!/bin/sh
# Host-side logic tests. No ESP toolchain needed.
cd "$(dirname "$0")"
set -e
for LAPEL in 0 1; do
  echo "--- mic tests (MIC_LAPEL_FITTED=$LAPEL) ---"
  g++ -std=gnu++17 -Wall -Wextra -DMIC_LAPEL_FITTED=$LAPEL -I . -I ../src test.cpp ../src/mic.cpp -o mic_test
  ./mic_test
done
echo "--- ui tests ---"
g++ -std=gnu++17 -Wall -Wextra -I . -I ../src test_ui.cpp -o ui_test && ./ui_test
