cmake_minimum_required(VERSION 3.7)
project(opencv_samples)

find_package(OpenCV REQUIRED)

get_filename_component(OpenCV_BINS "${OpenCV_LIB_PATH}/../bin" ABSOLUTE)
#file(TO_NATIVE_PATH "${OpenCV_BINS}" OpenCV_BINS)

message(STATUS "OpenCV_BINS: " ${OpenCV_BINS})
message(STATUS "CMAKE_SOURCE_DIR: " ${CMAKE_SOURCE_DIR})
message(STATUS "CMAKE_BINARY_DIR: " ${CMAKE_BINARY_DIR})
message(STATUS "CMAKE_INSTALL_PREFIX: " ${CMAKE_INSTALL_PREFIX})
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})
message(STATUS "CMAKE_CONFIGURATION_TYPES: " ${CMAKE_CONFIGURATION_TYPES})

# Add samples subdirectories
add_subdirectory(src/show_image)
