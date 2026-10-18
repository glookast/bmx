add_executable(test_op1a_data_tracks test_op1a_data_tracks.cpp)
target_link_libraries(test_op1a_data_tracks bmx MXFpp MXF)
target_include_directories(test_op1a_data_tracks PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/deps/libMXFpp
    ${PROJECT_SOURCE_DIR}/deps/libMXF
)

add_test(NAME op1a_data_tracks_unit_tests COMMAND test_op1a_data_tracks)
