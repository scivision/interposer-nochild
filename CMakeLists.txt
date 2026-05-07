cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

project(interposer-nochild LANGUAGES C)

option(BUILD_TESTING "Build tests" ON)

if(WIN32)
  enable_language(CXX)

  add_executable(no-children src/no-children_windows.cpp)
  target_compile_features(no-children PRIVATE cxx_std_23)

else()

  set(copts "$<$<CONFIG:Debug,RelWithDebInfo>:-Wall>")

  include(CheckPIESupported)
  check_pie_supported()
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

  add_library(no-children SHARED src/no-children_unix.c)
  target_compile_features(no-children PRIVATE c_std_99)
  target_compile_options(no-children PRIVATE ${copts})

if(APPLE)

  target_link_options(no-children PRIVATE -flat_namespace -undefined dynamic_lookup)

elseif(LINUX)

  target_link_libraries(no-children PRIVATE ${CMAKE_DL_LIBS})

else()
  message(FATAL_ERROR "Unsupported platform")
endif()
endif()

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()

file(GENERATE OUTPUT .gitignore CONTENT "*")
