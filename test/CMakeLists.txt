if(WIN32)

  add_executable(nochild-selftest nochild-selftest-windows.cpp)
  target_compile_features(nochild-selftest PRIVATE cxx_std_23)
  add_test(NAME ProcessLimitBlocking COMMAND no-children $<TARGET_FILE:nochild-selftest>)

elseif(UNIX)

  add_executable(nochild-selftest nochild-selftest.c)
  target_compile_features(nochild-selftest PRIVATE c_std_99)
  target_compile_options(nochild-selftest PRIVATE ${copts})

endif()

if(APPLE)

  add_test(NAME DYLD_INSERT_LIBRARIES
    COMMAND ${CMAKE_COMMAND} -E env DYLD_INSERT_LIBRARIES=$<TARGET_FILE:no-children> $<TARGET_FILE:nochild-selftest>
  )

  add_test(NAME DYLD_SIP_CONTROL COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test-sip-control.sh $<TARGET_FILE:no-children>)
  set_property(TEST DYLD_SIP_CONTROL PROPERTY SKIP_RETURN_CODE 77)

elseif(LINUX)

  add_test(NAME LD_PRELOAD
    COMMAND ${CMAKE_COMMAND} -E env LD_PRELOAD=$<TARGET_FILE:no-children> $<TARGET_FILE:nochild-selftest>
  )

endif()

# need to allow a little time for virus scanners, but avoid eternal hangs
get_property(test_names DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY TESTS)
set_property(TEST ${test_names} PROPERTY TIMEOUT 10)
