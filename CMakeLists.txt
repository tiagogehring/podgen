cmake_minimum_required(VERSION 3.16)
project(podgen)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(PodgenMacros)

option(BUILD_EXAMPLE "Build examples" OFF)
option(BUILD_TESTS "Build tests" OFF)

if (NOT(CAPNP_LIBRARIES))
    find_package(CapnProto REQUIRED)
    find_library(CAPNPC_LIBRARY capnpc)
endif()

capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS include/pod.capnp)

add_executable(podgen
    src/main.cpp
    src/PodGenerator.cpp
    src/PodGenerator.hpp
    src/reserved_words.hpp
    src/schema.cpp
    src/schema.hpp
    ${CAPNP_SRCS})

target_compile_features(podgen PRIVATE cxx_std_20)
target_include_directories(podgen PRIVATE ${CAPNP_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(podgen PRIVATE capnp capnpc)

#file(GLOB include_files "${CMAKE_CURRENT_SOURCE_DIR}/include/*")
#add_custom_target(copy_templates
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${include_files} ${CMAKE_CURRENT_BINARY_DIR}/include
#)
#add_dependencies(podgen copy_templates)

if (BUILD_EXAMPLE)
     add_subdirectory(example)
endif()
if (BUILD_TEST)
    add_subdirectory(test)
endif()
