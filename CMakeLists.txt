cmake_minimum_required(VERSION 3.25)


project(etna LANGUAGES CXX)

# Prevent the user from polluting the root directory with build files
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
    )
endif()

# For enabling debug stuff in "dev" builds, which are RelWithDebInfo.
if (NOT CMAKE_BUILD_TYPE STREQUAL Release)
  set(ETNA_DEBUG ON)
else ()
  set(ETNA_DEBUG OFF)
endif ()

# No profiling in release builds, mostly because Tracy emmits warnings in that case.
option(TRACY_ENABLE "Enable profiling" ${ETNA_DEBUG})

include("get_cpm.cmake")
include("thirdparty.cmake")
include("get_version.cmake")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(etna)
