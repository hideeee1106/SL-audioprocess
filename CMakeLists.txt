cmake_minimum_required(VERSION 3.21)
project(NKF_MNN_Deploy_R328)

set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_BUILD_TYPE Debug)


OPTION(BULID_IN_LINUX "OFF")
OPTION(BUILD_IN_R328 "OFF")

message("Build in MNN")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/)



#if(BUILD_IN_R328)
# mnn R328 infer
set(MNN_DIR /home/hideeee/3rparty/MNN/MNN/build-r328/mnn_install/include)
set(MNN_lIB_DIR /home/hideeee/3rparty/MNN/MNN/build-r328/mnn_install/lib)

find_package(PkgConfig REQUIRED)
set(PKG_CONFIG_PATH "/home/hideeee/CLionProjects/sphinxbase/install-r328-so/lib/pkgconfig:/home/hideeee/CLionProjects/pocketsphinx-main/build_r328_so/lib/pkgconfig")
# 使用 CMake 处理字符串，删除 blas 和 lapack

list(REMOVE_ITEM SPHINX_LIBRARIES blas lapack asound)


pkg_check_modules(SPHINX REQUIRED pocketsphinx sphinxbase)

# 添加包含目录
include_directories(${SPHINX_INCLUDE_DIRS})

# 设置库目录路径
link_directories(
        /home/hideeee/CLionProjects/sphinxbase/install-r328-so/lib
        /home/hideeee/CLionProjects/pocketsphinx-main/build_r328_so/lib
)


#elseif (BULID_IN_LINUX)
#
#set(MNN_DIR /home/hideeee/3rparty/MNN/linux/include)
#set(MNN_lIB_DIR /home/hideeee/3rparty/MNN/linux/lib)

#find_package(PkgConfig REQUIRED)
#pkg_check_modules(SPHINX REQUIRED pocketsphinx sphinxbase)
#include_directories(${SPHINX_INCLUDE_DIRS})
#link_directories(${SPHINX_LIBRARY_DIRS})

#endif ()

include_directories(${MNN_DIR})
link_directories(${MNN_lIB_DIR})
message("MNN Lib Path:${MNN_lIB_DIR}")
message("MNN DIR Path:${MNN_DIR}")

# MNN Libraries
set(MNN_LIBRARIES MNN)

# C_API comple files
set(C_API_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/c_api/c_api.cc)

# NS compile files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/ns/ NS_SRC_FILES)
message(STATUS "NS_SRC_FILES: ${NS_SRC_FILES}")

# Set compile files
set(COMPILE_FILES  ${C_API_FILES} ${NS_SRC_FILES})

message(STATUS "SPHINX_LIBRARIES=${SPHINX_LIBRARIES}")

# Build shared library
set(LIB_NAME AudioProcess-lite)
add_library(${LIB_NAME} SHARED ${COMPILE_FILES})
target_link_libraries(${LIB_NAME} ${MNN_LIBRARIES}  ${SPHINX_LIBRARIES})
message("build KARLMAN AEC with MNN on R328 ")

##build target
#add_executable(TestCAPINKFSample sample/test_nkf_capi.cpp ${COMPILE_FILES})
#target_link_libraries(TestCAPINKFSample ${MNN_LIBRARIES})
##
##
#add_executable(TestCAPINSSample sample/test_ns_capi.cpp ${COMPILE_FILES})
#target_link_libraries(TestCAPINSSample ${MNN_LIBRARIES})
#
#add_executable(TestCAPISample sample/test_capi_sample.cpp ${COMPILE_FILES})
#target_link_libraries(TestCAPISample ${MNN_LIBRARIES})
#
#add_executable(TestKWSCAPISample sample/test_kws_capi.cpp ${COMPILE_FILES})
#target_link_libraries(TestKWSCAPISample ${MNN_LIBRARIES} ${SPHINX_LIBRARIES})