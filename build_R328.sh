#!/bin/bash

# build

BUILD_DIR=./build_r328/

# 删除旧的构建目录，避免残留
if [[ -d "${BUILD_DIR}" ]]; then
  echo "Removing existing build directory: ${BUILD_DIR}"
  rm -rf "${BUILD_DIR}"
fi

# 创建新的构建目录
mkdir -p "${BUILD_DIR}"

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
