#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <format>
#include <cstdlib>

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << std::format("Assertion failed: {} at {}:{}\n", #cond, __FILE__, __LINE__); \
            std::exit(1); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            std::cerr << std::format("Assertion failed: {} == {} at {}:{}\n", #a, #b, __FILE__, __LINE__); \
            std::exit(1); \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        if (!((a) != (b))) { \
            std::cerr << std::format("Assertion failed: {} != {} at {}:{}\n", #a, #b, __FILE__, __LINE__); \
            std::exit(1); \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "Running " << #fn << "..." << std::endl; \
        fn(); \
        std::cout << "  OK" << std::endl; \
    } while (0)
