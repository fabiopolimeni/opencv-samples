set(SAMPLE_NAME show_gui)

add_executable(${SAMPLE_NAME} ${SAMPLE_NAME}.cpp)

# bgfx
target_link_libraries(${SAMPLE_NAME} bgfx bimg bx ib-compress ocornut-imgui example-common)
if(UNIX AND NOT APPLE)
    target_link_libraries(${SAMPLE_NAME} PUBLIC X11)
endif()

# OpenCV
target_link_libraries(${SAMPLE_NAME} ${OpenCV_LIBS})

set_property(TARGET ${SAMPLE_NAME} PROPERTY DEBUG_POSTFIX d)

install(TARGETS ${SAMPLE_NAME} DESTINATION bin)

if (CMAKE_HOST_WIN32)
    install(FILES $<TARGET_PDB_FILE:${SAMPLE_NAME}> DESTINATION bin OPTIONAL)
endif()
