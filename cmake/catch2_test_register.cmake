macro(catch2_test_register TEST_NAME TEST_FILE)
  add_executable(${TEST_NAME} ${TEST_FILE})
  target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)
  target_link_libraries(${TEST_NAME} PRIVATE
    ${PROJECT_NAME}
    Catch2::Catch2
    )

  add_test(
    NAME              ${TEST_NAME}
    COMMAND           ${TEST_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endmacro()
