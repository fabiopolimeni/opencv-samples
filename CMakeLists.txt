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

# Add dependencies
set(DEPS_DIR deps)

# bgfx
set(DEPS_BGFX_DIR ${DEPS_DIR}/bgfx)
add_subdirectory(${DEPS_BGFX_DIR})
include_directories(
    ${DEPS_BGFX_DIR}/bgfx/include
    ${DEPS_BGFX_DIR}/bimg/include
    ${DEPS_BGFX_DIR}/bx/include)

target_compile_definitions(bgfx PRIVATE "BGFX_CONFIG_RENDERER_OPENGL=41")

# Add samples
add_subdirectory(src/show_image)
add_subdirectory(src/show_camera)
add_subdirectory(src/show_gui)

# Copy shared libraries to destination folder
if (CMAKE_HOST_WIN32)
    file(GLOB OPENCV_DLLS "${OpenCV_BINS}/*.dll")
    message(STATUS "OPENCV_DLLS: " ${OPENCV_DLLS})
    install(FILES ${OPENCV_DLLS} DESTINATION bin)
endif()
