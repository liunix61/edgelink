#!/bin/bash

set -e
set -x

BASEDIR=$(dirname "$0")
pushd "$BASEDIR"

mkdir -p build

cmake -B build --preset "arm-linux-release" -DVCPKG_TARGET_TRIPLET=arm-linux-dynamic