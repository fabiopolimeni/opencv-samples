set(SAMPLE_NAME show_image)

add_executable(${SAMPLE_NAME} main.cpp)
target_link_libraries(${SAMPLE_NAME} ${OpenCV_LIBS})
set_property(TARGET ${SAMPLE_NAME} PROPERTY DEBUG_POSTFIX d)

install(TARGETS ${SAMPLE_NAME} DESTINATION bin)
install(FILES $<TARGET_PDB_FILE:${SAMPLE_NAME}> DESTINATION bin OPTIONAL)

# Copy shared libraries to destination folder
if (CMAKE_HOST_WIN32)
    file(GLOB OPENCV_DLLS "${OpenCV_BINS}/*.dll")
    message(STATUS "OPENCV_DLLS: " ${OPENCV_DLLS})
    install(FILES ${OPENCV_DLLS} DESTINATION bin)
endif()
