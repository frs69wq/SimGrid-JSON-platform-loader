#!/bin/bash
# Build the generated C++ platform into a loadable .so and validate it.
#   ./build_and_validate.sh
set -e
cd "$(dirname "$0")"

echo "[build] libfabric_us.so (the platform)"
g++ -shared -fPIC -std=c++17 -o libfabric_us.so fabric_us.cpp -lsimgrid

echo "[build] validate"
g++ -std=c++17 -O2 -o validate validate.cpp -lsimgrid

echo "[run] ./validate ./libfabric_us.so"
./validate ./libfabric_us.so
