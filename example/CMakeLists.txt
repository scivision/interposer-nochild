cmake_minimum_required(VERSION 3.19)

project(demoInterposter LANGUAGES NONE)


set(CMAKE_EXECUTE_PROCESS_COMMAND_ECHO STDOUT)

find_package(Git)
# uses execute_process() to get the Git version, should trigger log messages from our interposer.
