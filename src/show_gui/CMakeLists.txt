set(SAMPLE_NAME show_gui)

add_executable(${SAMPLE_NAME} ${SAMPLE_NAME}.cpp imgui_ext.cpp)
target_include_directories(${SAMPLE_NAME} PRIVATE .)

set_target_properties(${SAMPLE_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# bgfx
target_link_libraries(${SAMPLE_NAME} bgfx bimg bx ib-compress ocornut-imgui example-common)
if(UNIX AND NOT APPLE)
    target_link_libraries(${SAMPLE_NAME} X11)
endif()

# OpenCV
target_link_libraries(${SAMPLE_NAME} ${OpenCV_LIBS})

set_property(TARGET ${SAMPLE_NAME} PROPERTY DEBUG_POSTFIX d)

install(TARGETS ${SAMPLE_NAME} DESTINATION bin)

if (CMAKE_HOST_WIN32)
    install(FILES $<TARGET_PDB_FILE:${SAMPLE_NAME}> DESTINATION bin OPTIONAL)
endif()
