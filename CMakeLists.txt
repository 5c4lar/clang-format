cmake_minimum_required(VERSION 3.20)
project("clang-format")
set(LLVM_LINK_COMPONENTS
  Support
  )
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Clang REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG)


find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})
find_package(Python3 COMPONENTS Development)
include_directories(${Python3_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${LLVM_INCLUDE_DIRS})

add_library(clangformat
  ClangFormat.cpp
  )

target_link_libraries(
    clangformat
    PRIVATE
    clang-cpp
    LLVM
)

set_property(SOURCE ClangFormat.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE ClangFormat.i PROPERTY LANGUAGE CXX)
swig_add_library(clangformat_py LANGUAGE python SOURCES ClangFormat.cpp ClangFormat.i)

target_link_libraries(clangformat_py PRIVATE clangformat ${Python3_LIBRARIES})
set_target_properties(clangformat_py PROPERTIES PREFIX "")
set_target_properties(clangformat_py PROPERTIES OUTPUT_NAME "clang_format")

add_executable(clang_format_main
  main.cpp
  )

target_link_libraries(clang_format_main PRIVATE clangformat)