# test

catch2_test_register(test_url url.cpp)


set(ECHO_STRESS_WRK_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/stress_echo.lua)
configure_file(${CMAKE_CURRENT_LIST_DIR}/test_echo ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_test(
  NAME              test_echo
  COMMAND           test_echo
  WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  )
