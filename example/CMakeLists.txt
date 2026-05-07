cmake_minimum_required(VERSION 3.19)

project(demoInterposter LANGUAGES NONE)


set(CMAKE_EXECUTE_PROCESS_COMMAND_ECHO STDOUT)

find_package(Git)
# uses execute_process() to get the Git version, should trigger log messages from our interposer.

message(STATUS "Git version: ${Git_VERSION} ${GIT_VERSION_STRING}")

if(Git_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} --help
    RESULT_VARIABLE git_result
  )
    if(NOT git_result EQUAL 0)
        message(STATUS "PASS: Git help blocked as expected (exit code ${git_result})")
    else()
        message(WARNING "FAIL: Git help not blocked as expected (exit code ${git_result})")
    endif()

endif()
