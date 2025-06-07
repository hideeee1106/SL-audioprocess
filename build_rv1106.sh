#!/bin/bash

# build

BUILD_DIR=./build_rv1106/

if [[ ! -d "${BUILD_DIR}" ]]; then
  mkdir -p ${BUILD_DIR}
fi

GCC_COMPILER=/home/s4552/cross-compilation-toolchain/arm-rk-linux-toolchain/bin/arm-rockchip830-linux-uclibcgnueabihf
export STAGING_DIR=/home/s4552/cross-compilation-toolchain/arm-rk-linux-toolchain/


cd ${BUILD_DIR}
cmake .. \
    -DBUILD_IN_R328=OFF \
    -DBUILD_IN_RV1106=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_VERSION=1 \
    -DCMAKE_SYSTEM_PROCESSOR=armv7 \
    -DCMAKE_C_COMPILER=${GCC_COMPILER}-gcc \
    -DCMAKE_CXX_COMPILER=${GCC_COMPILER}-g++
make -j6

cd -
