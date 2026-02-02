#!/bin/bash

set -e
set -x

BASEDIR=$(dirname "$0")
pushd "$BASEDIR"

mkdir -p build

cmake -B build --preset "linux-debug" -DVCPKG_TARGET_TRIPLET=x64-linux-dynamic