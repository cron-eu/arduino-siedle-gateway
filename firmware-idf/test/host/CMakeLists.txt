# Host unit tests for hardware independent components.
#
#   cmake -S test/host -B build-host && cmake --build build-host && ctest --test-dir build-host --output-on-failure
#
# Uses the Unity test framework shipped with ESP-IDF, so IDF_PATH must be set.
cmake_minimum_required(VERSION 3.16)
project(siedle_gateway_host_tests C)

if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "IDF_PATH is not set, activate ESP-IDF first")
endif()

set(CMAKE_C_STANDARD 11)
set(UNITY_DIR $ENV{IDF_PATH}/components/unity/unity/src)
set(COMPONENTS_DIR ${CMAKE_CURRENT_LIST_DIR}/../../components)

add_library(unity STATIC ${UNITY_DIR}/unity.c)
target_include_directories(unity PUBLIC ${UNITY_DIR})

add_executable(test_siedle_proto
    test_siedle_proto.c
    ${COMPONENTS_DIR}/siedle_proto/siedle_proto.c
)
target_include_directories(test_siedle_proto PRIVATE ${COMPONENTS_DIR}/siedle_proto/include)
target_compile_options(test_siedle_proto PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_siedle_proto PRIVATE unity)

enable_testing()
add_test(NAME siedle_proto COMMAND test_siedle_proto)
