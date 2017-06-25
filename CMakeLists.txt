cmake_minimum_required(VERSION 3.8)
project(opencv_samples)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON) #...is required...

find_package(OpenCV REQUIRED)

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
if (MSVC)
    message(STATUS "MSVC_VERSION: " ${MSVC_VERSION})
    target_compile_definitions(bgfx PRIVATE "BGFX_CONFIG_RENDERER_DIRECT3D11=1")
    if (MSVC14 OR MSVC15)
        target_compile_definitions(bgfx PRIVATE "BGFX_CONFIG_RENDERER_DIRECT3D12=1")
    endif()
endif()    

# Add samples
add_subdirectory(src/show_image)
add_subdirectory(src/show_camera)
add_subdirectory(src/show_gui)

# Copy shared libraries to destination folder
if (CMAKE_HOST_WIN32)
    get_filename_component(OpenCV_BINS "${OpenCV_LIB_PATH}/../bin" ABSOLUTE)
    #file(TO_NATIVE_PATH "${OpenCV_BINS}" OpenCV_BINS)
    message(STATUS "OpenCV_BINS: " ${OpenCV_BINS})

    file(GLOB OPENCV_DLLS "${OpenCV_BINS}/*.dll")
    message(STATUS "OPENCV_DLLS: " ${OPENCV_DLLS})
    install(FILES ${OPENCV_DLLS} DESTINATION bin)
endif()
