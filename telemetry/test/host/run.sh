#!/bin/sh
# Build and run the host-side gear cross-check tests. Needs only a C++
# compiler — no PlatformIO, no hardware. (`pio run` ignores test/, and this
# is not a `pio test` suite.)
set -e
cd "$(dirname "$0")"
bin=$(mktemp)
trap 'rm -f "$bin"' EXIT
c++ -std=c++11 -Wall -Wextra -o "$bin" gear_crosscheck_test.cpp
"$bin"
