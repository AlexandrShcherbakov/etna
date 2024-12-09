find_package(Git)

if(GIT_EXECUTABLE)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )

  set(VERSION_REGEX "^v([0-9]+)\.([0-9]+)\.([0-9]+)$")
  if (${GIT_TAG} MATCHES ${VERSION_REGEX})
    string(REGEX REPLACE ${VERSION_REGEX} "\\1, \\2, \\3" ETNA_GIT_VERSION "${GIT_TAG}")
    return()
  endif()
endif()

set(ETNA_GIT_VERSION "0, 1, 0")
message("Failed to determine version from Git tags. Using default version 0.1.0")
