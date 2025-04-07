cmake_minimum_required(VERSION 3.21)
project(NKF_MNN_Deploy_R328)

set(CMAKE_CXX_STANDARD 11)


OPTION(BULID_IN_LINUX "OFF")
OPTION(BUILD_IN_R328 "OFF")

message("Build in MNN")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/)

#if(BUILD_IN_R328)
# mnn R328 infer
#set(MNN_DIR /home/hideeee/3rparty/MNN/MNN/build-r328/mnn_install/include)
#set(MNN_lIB_DIR /home/hideeee/3rparty/MNN/MNN/build-r328/mnn_install/lib)

#elseif (BULID_IN_LINUX)
#
set(MNN_DIR /home/hideeee/3rparty/MNN/linux/include)
set(MNN_lIB_DIR /home/hideeee/3rparty/MNN/linux/lib)
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


# Build shared library
set(LIB_NAME AudioProcess-lite)
add_library(${LIB_NAME} SHARED ${COMPILE_FILES})
target_link_libraries(${LIB_NAME} ${MNN_LIBRARIES})
message("build KARLMAN AEC with MNN on R328 ")

##build target
add_executable(TestCAPISample sample/test_audioprocesser.cpp ${COMPILE_FILES})
target_link_libraries(TestCAPISample ${MNN_LIBRARIES})
#
#add_executable(TestAECSample sample/test_nkf_aec.cpp ${COMPILE_FILES})
#target_link_libraries(TestAECSample ${MNN_LIBRARIES})
