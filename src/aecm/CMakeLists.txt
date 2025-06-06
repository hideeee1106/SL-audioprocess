# src/aecm/CMakeLists.txt

file(GLOB AECM_SRC
        ${CMAKE_CURRENT_LIST_DIR}/*.c
        ${CMAKE_CURRENT_LIST_DIR}/*.cc
)

# 过滤掉不需要的平台优化代码
list(FILTER AECM_SRC EXCLUDE REGEX ".*aecm_core_neon.cc$")
list(FILTER AECM_SRC EXCLUDE REGEX ".*aecm_core_mips.cc$")

# 设置位置无关代码编译选项
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 添加静态库
add_library(aecm STATIC ${AECM_SRC})

# 添加头文件目录
target_include_directories(aecm PUBLIC ${CMAKE_CURRENT_LIST_DIR})
