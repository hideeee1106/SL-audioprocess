#!/bin/bash

# build

BUILD_DIR=./build_r328/

if [[ ! -d "${BUILD_DIR}" ]]; then
  mkdir -p ${BUILD_DIR}
fi

GCC_COMPILER=/home/hideeee/3rparty/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi
export STAGING_DIR=/home/hideeee/3rparty/toolchain-sunxi-musl/toolchain/



cd ${BUILD_DIR}
cmake .. \
#    -DBUILD_IN_R328 = ON\
#    -BULID_IN_LINUX = OFF\
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=${GCC_COMPILER}-gcc \
    -DCMAKE_CXX_COMPILER=${GCC_COMPILER}-g++
make -j6

cd -
