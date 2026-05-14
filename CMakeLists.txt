cmake_minimum_required(VERSION 3.20)
project(xnetty VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Third-party dependencies ---
set(third_part_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_part")

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${third_part_dir}/googletest third_part/googletest-build EXCLUDE_FROM_ALL)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

# boringssl (SSL/TLS)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(${third_part_dir}/boringssl third_part/boringssl-build EXCLUDE_FROM_ALL)

# zlib (compression)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${third_part_dir}/zlib third_part/zlib-build EXCLUDE_FROM_ALL)

# --- Library ---
option(XNETTY_BUILD_SHARED "Build xnetty as shared library" OFF)
file(GLOB_RECURSE XNETTY_SRC "src/*.cpp")
# llhttp (HTTP parser, C)
set(llhttp_dir "${third_part_dir}/llhttp")
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(${llhttp_dir} third_part/llhttp-build EXCLUDE_FROM_ALL)

if(XNETTY_BUILD_SHARED)
    add_library(xnetty SHARED ${XNETTY_SRC})
else()
    add_library(xnetty STATIC ${XNETTY_SRC})
endif()
target_include_directories(xnetty PUBLIC include)
target_compile_options(xnetty PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(xnetty PUBLIC llhttp::llhttp ssl crypto zlibstatic)

# --- Tests ---
option(XNETTY_BUILD_TESTS "Build tests" ON)
if(XNETTY_BUILD_TESTS)
    enable_testing()

    file(GLOB TEST_SRC "tests/*_test.cpp" "tests/*_fuzz_test.cpp")
    foreach(TEST_FILE ${TEST_SRC})
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_FILE})
        target_link_libraries(${TEST_NAME} PRIVATE xnetty gtest_main)
        target_include_directories(${TEST_NAME} PRIVATE include)
        target_compile_definitions(${TEST_NAME} PRIVATE
            TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endforeach()
endif()

# --- Examples ---
file(GLOB EXAMPLE_SRC "examples/*.cpp")
foreach(EXAMPLE_FILE ${EXAMPLE_SRC})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_FILE})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE xnetty)
    target_include_directories(${EXAMPLE_NAME} PRIVATE include)
endforeach()

# --- Format (clang-format) ---
# 用法: 先 make tidy 再 make format (tidy 加括号后 format 对齐缩进)
find_program(CLANG_FORMAT clang-format
    HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
    /usr/lib/llvm-*/bin /usr/lib64/llvm-*/bin)
if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SRC
        "src/*.cpp" "include/**/*.h" "tests/*.cpp" "examples/*.cpp" "bench/*.cpp")
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i -style=file ${ALL_SRC}
        COMMENT "Formatting all source files with clang-format")
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT} --dry-run -Werror -style=file ${ALL_SRC}
        COMMENT "Checking code style (clang-format)")
endif()

# --- Generated config for clang-tidy (see cmake/tidy_config.h.in) ---
set(TIDY_CONFIG_H "${CMAKE_BINARY_DIR}/gen/tidy_config.h")
configure_file("${CMAKE_SOURCE_DIR}/cmake/tidy_config.h.in" "${TIDY_CONFIG_H}")

# --- Tidy (clang-tidy: 强制大括号) ---
# 先于 format 执行: tidy 加括号, format 整理缩进
# make tidy && make format
find_program(CLANG_TIDY clang-tidy
    HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
    /usr/lib/llvm-*/bin /usr/lib64/llvm-*/bin)
if(CLANG_TIDY)
    file(GLOB_RECURSE TIDY_SRC "src/*.cpp" "tests/*.cpp" "examples/*.cpp")
    if(XNETTY_BUILD_BENCH)
        file(GLOB_RECURSE BENCH_TIDY_SRC "bench/*.cpp")
        list(APPEND TIDY_SRC ${BENCH_TIDY_SRC})
    endif()
    set(TIDY_CHECKS "-*,readability-braces-around-statements,readability-else-after-return")
    set(TIDY_HEADER_FILTER "^${CMAKE_SOURCE_DIR}/(src|include|tests|examples|bench)/")
    set(TIDY_FLAGS --fix-errors --header-filter='${TIDY_HEADER_FILTER}' -- -I${CMAKE_SOURCE_DIR}/include
        -I${CMAKE_SOURCE_DIR}/third_part/llhttp/include
        -I${CMAKE_SOURCE_DIR}/third_part/boringssl/include
        -I${CMAKE_SOURCE_DIR}/third_part/zlib
        -I${CMAKE_BINARY_DIR}/third_part/zlib-build
        -I${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
        -I${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
        "-include${TIDY_CONFIG_H}"
        -std=c++17)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY} --checks='${TIDY_CHECKS}' --fix ${TIDY_SRC}
            ${TIDY_FLAGS}
        COMMENT "Running clang-tidy (braces, style fixes)")
    add_custom_target(tidy-check
        COMMAND ${CLANG_TIDY} --checks='${TIDY_CHECKS}' --warnings-as-errors='*' ${TIDY_SRC}
            ${TIDY_FLAGS}
        COMMENT "Checking clang-tidy compliance")
endif()

# --- Install (lib + headers) ---
# 用法 / usage: cmake --install build --prefix /usr/local
install(TARGETS xnetty
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)
install(DIRECTORY include/xnetty
    DESTINATION include
    FILES_MATCHING PATTERN "*.h")
if(NOT XNETTY_BUILD_SHARED)
    install(TARGETS llhttp_static ssl crypto zlibstatic
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
    install(FILES ${third_part_dir}/llhttp/include/llhttp.h
        DESTINATION include)
    install(DIRECTORY ${third_part_dir}/boringssl/include/openssl
        DESTINATION include)
    install(FILES
        ${third_part_dir}/zlib/zlib.h
        ${CMAKE_BINARY_DIR}/third_part/zlib-build/zconf.h
        DESTINATION include)
endif()

# --- Benchmarks ---
option(XNETTY_BUILD_BENCH "Build benchmarks" OFF)
if(XNETTY_BUILD_BENCH)
    add_subdirectory(${third_part_dir}/benchmark third_part/benchmark-build EXCLUDE_FROM_ALL)

    file(GLOB BENCH_SRC "bench/*_bench.cpp")
    foreach(BENCH_FILE ${BENCH_SRC})
        get_filename_component(BENCH_NAME ${BENCH_FILE} NAME_WE)
        add_executable(${BENCH_NAME} ${BENCH_FILE})
        target_link_libraries(${BENCH_NAME} PRIVATE xnetty benchmark::benchmark)
        target_include_directories(${BENCH_NAME} PRIVATE include)
    endforeach()
endif()
