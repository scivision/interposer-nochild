cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

project(interposer-nochild LANGUAGES C CXX)

option(BUILD_TESTING "Build tests" ON)

if(BUILD_TESTING)
  enable_testing()
endif()

if(WIN32)
  add_executable(no-children no-children_windows.cpp)
  target_compile_features(no-children PRIVATE cxx_std_23)

  if(BUILD_TESTING)
    add_executable(nochild-selftest test/nochild-selftest-windows.cpp)
    target_compile_features(nochild-selftest PRIVATE cxx_std_23)
    add_test(NAME ProcessLimitBlocking
      COMMAND no-children $<TARGET_FILE:nochild-selftest>
    )
  endif()
else()

  set(copts "$<$<CONFIG:Debug,RelWithDebInfo>:-Wall>")

  include(CheckPIESupported)
  check_pie_supported()
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

  add_library(no-children SHARED no-children_unix.c)
  target_compile_features(no-children PRIVATE c_std_99)
  target_compile_options(no-children PRIVATE ${copts})

  if(BUILD_TESTING)
    add_executable(nochild-selftest test/nochild-selftest.c)
    target_compile_features(nochild-selftest PRIVATE c_std_99)
    target_compile_options(nochild-selftest PRIVATE ${copts})
  endif()

if(APPLE)

  target_link_options(no-children PRIVATE -flat_namespace -undefined dynamic_lookup)

  add_test(NAME DYLD_INSERT_LIBRARIES
    COMMAND ${CMAKE_COMMAND} -E env DYLD_INSERT_LIBRARIES=$<TARGET_FILE:no-children> $<TARGET_FILE:nochild-selftest>
  )

  add_test(NAME DYLD_SIP_CONTROL COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/test-sip-control.sh $<TARGET_FILE:no-children>)

elseif(LINUX)

  target_link_libraries(no-children PRIVATE ${CMAKE_DL_LIBS})

  add_test(NAME LD_PRELOAD
    COMMAND ${CMAKE_COMMAND} -E env LD_PRELOAD=$<TARGET_FILE:no-children> $<TARGET_FILE:nochild-selftest>
  )

else()
  message(FATAL_ERROR "Unsupported platform")
endif()
endif()

file(GENERATE OUTPUT .gitignore CONTENT "*")
