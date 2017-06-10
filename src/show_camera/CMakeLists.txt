set(SAMPLE_NAME show_camera)

add_executable(${SAMPLE_NAME} main.cpp)
target_link_libraries(${SAMPLE_NAME} ${OpenCV_LIBS})
set_property(TARGET ${SAMPLE_NAME} PROPERTY DEBUG_POSTFIX d)

install(TARGETS ${SAMPLE_NAME} DESTINATION bin)
install(FILES $<TARGET_PDB_FILE:${SAMPLE_NAME}> DESTINATION bin OPTIONAL)
