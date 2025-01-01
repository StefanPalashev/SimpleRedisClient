cmake_minimum_required(VERSION 3.10)
project(SimpleRedisClient)

# Enable testing
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# Specify the clang++ compiler explicitly
set(CMAKE_CXX_COMPILER "/usr/bin/clang++-11")

# Compiler flags for Debug and Release
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -fstandalone-debug -fdiagnostics-color=always")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -fdiagnostics-color=always")

# Add the directory containing hiredis
link_directories(/usr/local/lib)

# Add the include directories for hiredis
include_directories(/usr/local/include)

# Find GoogleTest
find_package(GTest REQUIRED)

# Configure output directory
set(OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)

# Recursively find all .cpp, .hpp, .cu, .c, and .h files in the source and include directories
file(GLOB_RECURSE SOURCES src/redis_client.cpp
"src/**/*.cpp" 
"src/**/*.hpp" 
"include/**/*.cpp" 
"include/**/*.hpp" 
"src/**/*.cu" 
"src/**/*.c" 
"include/**/*.c" 
"include/**/*.h")

# Define the main executable target
add_executable(simple_redis_client ${SOURCES})

# Link hiredis with executable
target_link_libraries(simple_redis_client hiredis)

#Define the executable target for the test binary
add_executable(test_json_message_processor src/Consumer/JsonMessageProcessorImpl.cpp tests/test_json_message_processor.cpp)

#Link GoogleTest libraries to the test binary
target_link_libraries(test_json_message_processor gtest gtest_main)

#Define the test for RedisConsumer
add_executable(test_redis_consumer_apis src/Consumer/RedisConsumer.cpp src/Consumer/JsonMessageProcessorImpl.cpp tests/test_redis_consumer_apis.cpp)

target_link_libraries(test_redis_consumer_apis gtest gtest_main hiredis)

# Enable the test for CTest
add_test(NAME JsonMessageProcessorTest COMMAND test_json_message_processor)
add_test(NAME RedisConsumerAPITest COMMAND test_redis_consumer_apis)

# Specify the output directory for the binaries
set_target_properties(simple_redis_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}
)

set_target_properties(test_json_message_processor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}
)

set_target_properties(test_redis_consumer_apis PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}
)

# Create a custom target to format code with clang-format
add_custom_target(
    format ALL
    COMMAND clang-format -style=file -i ${SOURCES}
    COMMENT "Running clang-format on source files"
)

# Add the custom format target as a dependency for the build
# This ensures that code is formatted every time make is being run.
add_dependencies(simple_redis_client format)

# Optionally, create a custom target for running tests
add_custom_target(run_tests
    COMMAND test_json_message_processor
    COMMAND test_redis_consumer_apis
    DEPENDS test_json_message_processor test_redis_consumer_apis
    COMMENT "Running the test binary"
)
