cmake_minimum_required(VERSION 3.21)
project(sudoku_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SUDOKU_WERROR "Treat compiler warnings as errors" OFF)
option(SUDOKU_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})

add_library(sudoku
  src/api.cpp
  src/board.cpp
  src/dlx.cpp
  src/generator.cpp
  src/grid.cpp
  src/logic.cpp
  src/naive.cpp
  src/smart.cpp
  src/techniques.cpp
)
target_include_directories(sudoku PUBLIC include)
target_compile_definitions(sudoku PRIVATE SUDOKU_VERSION="${PROJECT_VERSION}")
target_compile_options(sudoku PRIVATE -Wall -Wextra -Wpedantic -Wshadow $<$<BOOL:${SUDOKU_WERROR}>:-Werror>)

if(EMSCRIPTEN)
  # ES module + .wasm, loadable from the page, a web worker or Node.
  add_executable(sudoku_wasm wasm/bindings.cpp)
  target_link_libraries(sudoku_wasm PRIVATE sudoku)
  set_target_properties(sudoku_wasm PROPERTIES OUTPUT_NAME sudoku SUFFIX ".mjs")
  target_link_options(sudoku_wasm PRIVATE
    -lembind
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sEXPORT_NAME=createSudokuEngine
    -sENVIRONMENT=web,worker,node
    -sALLOW_MEMORY_GROWTH=1
    -sSTACK_SIZE=1MB
    -sFILESYSTEM=0
    "SHELL:--emit-tsd ${CMAKE_CURRENT_BINARY_DIR}/sudoku.d.mts"
  )
  return()
endif()

add_executable(sudoku_cli cli/main.cpp)
target_link_libraries(sudoku_cli PRIVATE sudoku)
set_target_properties(sudoku_cli PROPERTIES OUTPUT_NAME sudoku)
target_compile_options(sudoku_cli PRIVATE -Wall -Wextra -Wpedantic -Wshadow $<$<BOOL:${SUDOKU_WERROR}>:-Werror>)

if(SUDOKU_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(sudoku_tests
    tests/api_test.cpp
    tests/generator_test.cpp
    tests/grid_test.cpp
    tests/logic_test.cpp
    tests/search_test.cpp
  )
  target_link_libraries(sudoku_tests PRIVATE sudoku GTest::gtest_main)
  target_compile_options(sudoku_tests PRIVATE -Wall -Wextra $<$<BOOL:${SUDOKU_WERROR}>:-Werror>)
  include(GoogleTest)
  gtest_discover_tests(sudoku_tests)
endif()
