# cmake_sandbox.cmake - Run CMake with child-process blocking (cross-platform)

cmake_minimum_required(VERSION 3.25)

function(print_help)
  message(STATUS "Usage: cmake -Dsource=<project_source_dir> -Dcmake=<path to cmake exe> [-Dlib=<path to no-children library or executable>] -P cmake_sandbox.cmake")
endfunction()

option(debug "--debug-output")
option(trace "--trace-expand")

if(NOT DEFINED source)
  set(source ${CMAKE_CURRENT_LIST_DIR})
endif()

find_program(cmake
NAMES cmake
HINTS ${source}
PATH_SUFFIXES bin build/bin
)
if(NOT cmake)
  set(cmake ${CMAKE_COMMAND})
endif()

if(NOT DEFINED mode)
  # only relevant for macOS at this time
  set(mode "dylib")
endif()

if(WIN32)
  find_program(nochildren NAMES no-children
  PATHS "${CMAKE_CURRENT_LIST_DIR}/build"
  HINTS ${lib}
  NO_DEFAULT_PATH
  REQUIRED
  )
elseif(mode STREQUAL "dylib")
  find_library(nochildren NAMES no-children
  PATHS "${CMAKE_CURRENT_LIST_DIR}/build"
  HINTS ${lib}
  NO_DEFAULT_PATH
  REQUIRED
  )
endif()

if(NOT DEFINED CMAKE_GENERATOR)
if(DEFINED ENV{CMAKE_GENERATOR})
  set(CMAKE_GENERATOR $ENV{CMAKE_GENERATOR})
else()
  set(CMAKE_GENERATOR "Ninja")
endif()
endif()

set(opts "-DCMAKE_EXECUTE_PROCESS_COMMAND_ERROR_IS_FATAL=ANY")

if(DEFINED ENV{TEMP})
  set(BDIR $ENV{TEMP}/build_cmake_sandbox)
elseif(DEFINED ENV{TMPDIR})
  set(BDIR $ENV{TMPDIR}/build_cmake_sandbox)
else()
  set(BDIR /tmp/build_cmake_sandbox)
endif()

set(base_cmd ${cmake} -B${BDIR} -S${source} ${CEXE} --fresh -G "${CMAKE_GENERATOR}" ${opts})
if(trace)
  list(APPEND base_cmd "--trace-expand")
endif()
if(debug)
  list(APPEND base_cmd "--debug-output")
endif()

if(WIN32)
  set(full_cmd ${nochildren} ${base_cmd}
    -DCMAKE_CXX_COMPILER_WORKS:BOOL=yes
    -DCMAKE_C_COMPILER_WORKS:BOOL=yes
    -DCMake_HAVE_CXX_UNIQUE_PTR:BOOL=yes
    -DCMAKE_SIZEOF_VOID_P=8
  )
elseif(APPLE)
  if(mode STREQUAL "sandbox")
    set(full_cmd sandbox-exec -f ${CMAKE_CURRENT_LIST_DIR}/sandbox.cfg ${base_cmd})
  else()
    set(full_cmd ${CMAKE_COMMAND} -E env
      --unset=DYLD_PRINT_LIBRARIES
      --unset=DYLD_PRINT_APIS
      --unset=DYLD_PRINT_OPTS
      DYLD_INSERT_LIBRARIES=${nochildren}
     ${base_cmd})
  endif()
elseif(UNIX)
  set(full_cmd ${CMAKE_COMMAND} -E env LD_PRELOAD=${nochildren} ${base_cmd})
else()
  message(FATAL_ERROR "Unsupported platform")
endif()

execute_process(COMMAND ${full_cmd}
  RESULT_VARIABLE run_result
  COMMAND_ERROR_IS_FATAL ANY
  COMMAND_ECHO STDOUT
)
