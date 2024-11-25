find_package(Git)

if(GIT_EXECUTABLE)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE ETNA_VERSION
    RESULT_VARIABLE ERROR_CODE
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if (${ETNA_VERSION})
    string(REGEX MATCHALL "[0-9]+" NUM_LIST ${ETNA_VERSION}) 
    list(LENGTH NUM_LIST LEN)

    if (LEN STREQUAL "3")
      list(GET NUM_LIST 0 TMP)
      add_definitions(-DETNA_MAJOR=${TMP})
      list(GET NUM_LIST 1 TMP)
      add_definitions(-DETNA_MINOR=${TMP})
      list(GET NUM_LIST 2 TMP)
      add_definitions(-DETNA_PATCH=${TMP})
      return()
    endif()
  endif()
endif()

add_definitions(-DETNA_MAJOR=0)
add_definitions(-DETNA_MINOR=1)
add_definitions(-DETNA_PATCH=0)

message("Failed to determine version from Git tags. Using default version 0.1.0")
