# cmake

cmake_minimum_required(VERSION 3.12)

if(PROJECT_NAME)
  set(IS_SUBPROJECT TRUE)
endif()

project(restpp)


include(cmake/doxygen.cmake)
include(cmake/version.cmake)
include(cmake/catch2_test_register.cmake)

if(NOT IS_SUBPROJECT)
  include(cmake/build.cmake)
endif()


message(STATUS "restpp version: ${PROJECT_VERSION}")


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


add_executable(echo_service example/main.cpp example/stats_service.cpp)
target_link_libraries(echo_service PRIVATE
  ${PROJECT_NAME}
  ${Boost_LIBRARIES}
  Threads::Threads
  dl # NOTE: this library must links after all other libraries!
  )


if(BUILD_TESTING)
  add_subdirectory(test)
endif()
