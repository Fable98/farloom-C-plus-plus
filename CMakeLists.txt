cmake_minimum_required(VERSION 3.20)
project(farloom VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags and warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wno-unused-parameter)
endif()

find_package(Threads REQUIRED)

include_directories(include)

# Core library
set(FARLOOM_LIB_SOURCES
    src/logger.cpp
    src/config.cpp
    src/mime.cpp
    src/static_files.cpp
    src/router.cpp
    src/routes.cpp
    src/thread_pool.cpp
    src/connection.cpp
    src/server.cpp
    src/http/request.cpp
    src/http/response.cpp
    src/http/percent.cpp
    src/http/parser.cpp
)

add_library(farloom_lib STATIC ${FARLOOM_LIB_SOURCES})
target_link_libraries(farloom_lib PUBLIC Threads::Threads)
target_include_directories(farloom_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Server executable (farloom / webserver)
add_executable(farloom src/main.cpp)
target_link_libraries(farloom PRIVATE farloom_lib)
# Also create alias or symlink 'webserver' for compatibility
add_custom_target(webserver_symlink ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink farloom webserver
    DEPENDS farloom
)

# Loadtest library and executable
add_library(loadtest_lib STATIC
    loadtest/args.cpp
    loadtest/stats.cpp
)
target_include_directories(loadtest_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/loadtest>
)
target_link_libraries(loadtest_lib PUBLIC Threads::Threads)

add_executable(loadtest
    loadtest/main.cpp
)
target_link_libraries(loadtest PRIVATE loadtest_lib)

# Testing
enable_testing()

macro(add_farloom_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE farloom_lib)
    target_include_directories(${name} PRIVATE tests)
    add_test(NAME ${name} COMMAND ${name})
endmacro()

macro(add_loadtest_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE loadtest_lib)
    target_include_directories(${name} PRIVATE tests loadtest)
    add_test(NAME ${name} COMMAND ${name})
endmacro()

# Unit tests
add_farloom_test(test_config tests/unit/test_config.cpp)
add_farloom_test(test_percent tests/unit/test_percent.cpp)
add_farloom_test(test_http_parser tests/unit/test_http_parser.cpp)
add_farloom_test(test_http_response tests/unit/test_http_response.cpp)
add_farloom_test(test_mime tests/unit/test_mime.cpp)
add_farloom_test(test_router tests/unit/test_router.cpp)
add_farloom_test(test_static_files tests/unit/test_static_files.cpp)
add_farloom_test(test_thread_pool tests/unit/test_thread_pool.cpp)
add_farloom_test(test_logger tests/unit/test_logger.cpp)

add_loadtest_test(test_loadtest_args tests/unit/test_loadtest_args.cpp)
add_loadtest_test(test_loadtest_stats tests/unit/test_loadtest_stats.cpp)

# Integration tests
add_farloom_test(test_integration tests/integration/test_integration.cpp)
add_farloom_test(test_lifecycle tests/integration/test_lifecycle.cpp)
