cmake_minimum_required(VERSION 3.20)


project(etna)

# Prevent the user from polluting the root directory with build files
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
    )
endif()


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (MSVC)
  add_compile_options(/W3)
else()
  add_compile_options(-Wall -Wextra)
endif()


include("get_cpm.cmake")
include("thirdparty.cmake")


add_subdirectory(etna)
