add_executable(test_hevc_op1a test_hevc_op1a.cpp)
target_link_libraries(test_hevc_op1a bmx MXFpp MXF)
target_include_directories(test_hevc_op1a PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/deps/libMXFpp
    ${PROJECT_SOURCE_DIR}/deps/libMXF
)

add_test(NAME hevc_op1a_unit_tests COMMAND test_hevc_op1a)
