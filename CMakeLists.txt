# cmake

cmake_minimum_required(VERSION 3.12)

project(restpp)


include(cmake/build.cmake)
include(cmake/doxygen.cmake)
include(cmake/catch2_test_register.cmake)
#include(cmake/version.cmake)


add_subdirectory(third-party)
find_package(Threads REQUIRED)
find_package(Boost COMPONENTS program_options stacktrace_backtrace REQUIRED)


set(PROJECT_SRC
  src/server.cpp
  src/http/url.cpp
  )

add_library(${PROJECT_NAME} ${PROJECT_SRC})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_options(${PROJECT_NAME} PRIVATE -g)
target_include_directories(${PROJECT_NAME} PUBLIC
  include
  ${Boost_INCLUDE_DIRS}
  )
target_link_libraries(${PROJECT_NAME} PRIVATE
  Threads::Threads
  ${Boost_LIBRARIES}
  )
target_link_libraries(${PROJECT_NAME} PUBLIC
  Boost::boost
  simple_logs
  )


add_executable(echo_service example/main.cpp)
target_link_options(echo_service PRIVATE -ldl)
target_link_libraries(echo_service PRIVATE
  ${PROJECT_NAME}
  ${Boost_LIBRARIES}
  Threads::Threads
  )


if(BUILD_TESTING)
  add_subdirectory(test)
endif()
