cmake_minimum_required(VERSION 3.21)
project(NKF_MNN_Deploy_R328)

set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_BUILD_TYPE Debug)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon -march=armv7-a")
#set(CMAKE_CXX_FLAGS "-march=armv7-a -mfloat-abi=hard -mfpu=neon-vfpv4 ${CMAKE_CXX_FLAGS}")
#set(CMAKE_C_FLAGS "-march=armv7-a -mfloat-abi=hard -mfpu=neon-vfpv4 ${CMAKE_C_FLAGS}" )
#message(STATUS "CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
#include_directories(
#        /home/hideeee/3rparty/toolchain-sunxi-musl/toolchain/lib/gcc/arm-openwrt-linux-muslgnueabi/6.4.1/include
#)

message(STATUS "BUILD_IN_R328: ${BUILD_IN_R328}")
message(STATUS "BUILD_IN_RV1106: ${BUILD_IN_RV1106}")


OPTION( BUILD_IN_R328 "Build in r328" OFF)
OPTION( BUILD_IN_RV1106 "Build in RV1106" ON)

message(STATUS "BUILD_IN_R328: ${BUILD_IN_R328}")
message(STATUS "BUILD_IN_RV1106: ${BUILD_IN_RV1106}")


message("Build in MNN")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/)





if(NOT BUILD_IN_R328 AND NOT BUILD_IN_RV1106)
#    fftw

    set(MNN_DIR /media/s4552/F3CF5497F0925C7D/3rdparty/MNN/include)
    set(MNN_LIB_DIR /media/s4552/F3CF5497F0925C7D/3rdparty/MNN/build/express)
    include_directories(/media/s4552/F3CF5497F0925C7D/3rdparty/fftw3/linux/include)
    set(FFTW3_LIBRARIES /media/s4552/F3CF5497F0925C7D/3rdparty/fftw3/linux/lib/libfftw3f.a)

elseif (BUILD_IN_RV1106)
    set(MNN_DIR /home/hideeee/3rparty/MNN/build_armv7_rv1106/include)
    set(MNN_LIB_DIR /home/hideeee/3rparty/MNN/build_armv7_rv1106/lib)

    include_directories(/home/hideeee/3rparty/fftw3/armv7_830_f/include)
    set(FFTW3_LIBRARIES /home/hideeee/3rparty/fftw3/armv7_830_f/lib/libfftw3f.a)


else ()
    #    fftw
    include_directories(/home/hideeee/3rparty/fftw3/armv7_r328/install/include)
    set(FFTW3_LIBRARIES /home/hideeee/3rparty/fftw3/armv7_r328/install/lib/libfftw3f.a)

    set(MNN_DIR /home/hideeee/3rparty/MNN/r328/include)
    set(MNN_LIB_DIR /home/hideeee/3rparty/MNN/r328/lib)

endif ()

include_directories(${MNN_DIR})
link_directories(${MNN_LIB_DIR})
message("MNN Lib Path:${MNN_LIB_DIR}")
message("MNN DIR Path:${MNN_DIR}")

# MNN Libraries
set(MNN_LIBRARIES MNN)

# C_API comple files
set(C_API_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/c_api/c_api.cc)

# NS compile files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/ns/ NS_SRC_FILES)
message(STATUS "NS_SRC_FILES: ${NS_SRC_FILES}")

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/kws      KWS_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/frontend FRONTEND_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/until    UNTIL_SRC)
set(KWS_FILES ${KWS_SRC} ${FRONTEND_SRC} ${UNTIL_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/src/KwsPipeline.cpp)


# 添加 AECM 子目录
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/aecm)


# Set compile files
set(COMPILE_FILES  ${C_API_FILES} ${NS_SRC_FILES} ${KWS_FILES})


# Build shared library
set(LIB_NAME AudioProcess-lite)
add_library(${LIB_NAME} SHARED ${COMPILE_FILES})
target_link_libraries(${LIB_NAME} ${MNN_LIBRARIES} ${FFTW3_LIBRARIES} aecm)
message("build KARLMAN AEC with MNN on R328 ")

##build target
add_executable(TestCAPINKFSample sample/test_nkf_capi.cpp ${COMPILE_FILES})
target_link_libraries(TestCAPINKFSample ${MNN_LIBRARIES} ${FFTW3_LIBRARIES})
##
##
add_executable(TestCAPINSSample sample/test_ns_capi.cpp ${COMPILE_FILES})
target_link_libraries(TestCAPINSSample ${MNN_LIBRARIES} ${FFTW3_LIBRARIES})
#
#add_executable(TestCAPISample sample/test_capi_sample.cpp ${COMPILE_FILES})
#target_link_libraries(TestCAPISample ${MNN_LIBRARIES})
#
#add_executable(TestKWSCAPISample sample/test_wekws_capi.cpp ${COMPILE_FILES})
#target_link_libraries(TestKWSCAPISample ${MNN_LIBRARIES} )

add_executable(TestKWSsignleCAPISample sample/test_signlekws.cpp ${COMPILE_FILES})
target_link_libraries(TestKWSsignleCAPISample ${MNN_LIBRARIES} ${FFTW3_LIBRARIES} aecm)