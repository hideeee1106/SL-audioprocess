#!/bin/bash

# build

BUILD_DIR=./build_r328/

if [[ ! -d "${BUILD_DIR}" ]]; then
  mkdir -p ${BUILD_DIR}
fi

GCC_COMPILER=/home/s4552/cross-compilation-toolchain/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi
export STAGING_DIR=/home/s4552/cross-compilation-toolchain/toolchain-sunxi-musl/toolchain



cd ${BUILD_DIR}
cmake .. \
    -DBUILD_IN_R328=ON \
    -DBUILD_IN_RV1106=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=${GCC_COMPILER}-gcc \
    -DCMAKE_CXX_COMPILER=${GCC_COMPILER}-g++
make -j6

cd -
