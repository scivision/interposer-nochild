@echo off
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "GENERATOR="
set "SRC=."

set CEXE="-DCMAKE_EXECUTE_PROCESS_COMMAND_ERROR_IS_FATAL=ANY"

:parse_args
if "%~1"=="" goto args_done

if /I "%~1"=="-G" (
    if "%~2"=="" (
        echo Missing value for -G
        exit /b 1
    )
    set "GENERATOR=-G "%~2""
    shift
    shift
    goto parse_args
)

if /I "%~1"=="-S" (
    if "%~2"=="" (
        echo Missing value for -S
        exit /b 1
    )
    set "SRC=%~2"
    shift
    shift
    goto parse_args
)

echo Unknown argument: %~1
echo Usage: %~nx0 [-G "Generator Name"] [-S source_dir]
exit /b 1

:args_done

for /f "tokens=* usebackq" %%i in (`where clang.exe 2^>nul`) do set "CC=%%i" & goto found_cc
:found_cc
for /f "tokens=* usebackq" %%i in (`where clang++.exe 2^>nul`) do set "CXX=%%i" & goto found_cxx
:found_cxx

set "CVARS=-DCMAKE_CXX_COMPILER_WORKS=yes -DCMAKE_C_COMPILER_WORKS=yes"
set "CVARS=%CVARS% -DCMAKE_C_COMPILER=%CC%"
set "CVARS=%CVARS% -DCMAKE_CXX_COMPILER=%CXX%"
set "CVARS=%CVARS% -DCMake_HAVE_CXX_UNIQUE_PTR=yes"


if exist "%SCRIPT_DIR%\build\bin\cmake.exe" (
    set "CMAKE_EXE=%SCRIPT_DIR%\build\bin\cmake.exe"
) else (
	set "CMAKE_EXE=cmake.exe"
)

set "BUILD_DIR=%TEMP%\build_cmake_sandbox"

@echo on
"%SCRIPT_DIR%\nochild.exe" "%CMAKE_EXE%" %GENERATOR% -B "%BUILD_DIR%" -S "%SRC%" %CVARS% %CEXE% --fresh
