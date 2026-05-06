# Interposer to deny child process launch

[![ci](https://github.com/scivision/interposer-nochild/actions/workflows/ci.yml/badge.svg)](https://github.com/scivision/interposer-nochild/actions/workflows/ci.yml)

When testing a large program for development, it is useful to deny the program the ablility to execute child processes to help enusre the top program is stable in such cases.
This technique is demonstrated for Linux, macOS, and Windows on compilers including GCC, Clang, MSVC, NVHPC, and oneAPI.
This is not a cybersecurity sandbox, but rather a limited development tool to test stability of the top program when child process launch fails.
Actual
[sandboxing](https://hardenedlinux.org/blog/2024-08-20-gnu/linux-sandboxing-a-brief-review/),
tools for the platform also can deny access to resources including filesystem, network, and/or child processes such as
[Linux Firejail](https://github.com/netblue30/firejail),
[macOS App Sandbox](https://developer.apple.com/documentation/xcode/configuring-the-macos-app-sandbox),
or
[Windows Sandbox](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/).

## Preparation

This project was designed for the specific purpose of aiding in CMake internal development of CMake itself.
It can be used with generally any program, but the examples here are all for CMake itself.

We assume the local development
[source code of CMake](https://gitlab.kitware.com/cmake/cmake)
has already been built like:

```sh
BUILDDIR=/tmp/build
SOURCE=/path/to/cmake/source

cmake -S "$SOURCE" -B "$BUILDDIR"
cmake --build "$BUILDDIR"
```

Specify the path to this CMake executable using the scripts below with option like `-c $BUILDDIR/bin/cmake -S $SOURCE`.

## Linux

First, build and test our interposer library `./build/libno-children.so`:

```sh
cmake --workflow --preset default
```

A dynamic library with `LD_PRELOAD` can be used to interpose system calls and deny child process launch.

Use interposer

```sh
./cmake_sandbox.sh dylib -c $BUILDDIR/bin/cmake -S $SOURCE
```

Separately, the [seccomp](./seccomp) directory shows use of Linux seccomp to deny child process launch.

## macOS

First, build and test our interposer library `./build/libno-children.dylib`:

```sh
cmake --workflow default
```

Akin to Linux `LD_PRELOAD`, on macOS
[DYLD_INSERT_LIBRARIES](https://theevilbit.github.io/posts/dyld_insert_libraries_dylib_injection_in_macos_osx_deep_dive/)
can be used to interpose system calls thereby denying child process launch.
This has the usual SIP limitations for intercepting system process calls.

Use interposer

```sh
./cmake_sandbox.sh dylib -c $BUILDDIR/bin/cmake -S $SOURCE
```

### sandbox-exec

[sandbox-exec](https://man.freebsd.org/cgi/man.cgi?query=sandbox-exec&apropos=0&sektion=1&manpath=macOS+26.4&format=html)
can lockup macOS if there is an infinite loop in the program when child process launch is denied.
One can use this interposer dylib instead on macOS.

Sandbox-exec may lockup macOS requiring hard reboot if there is an infinite loop or recursion in the program when child process launch is denied.

```sh
./cmake_sandbox.sh sandbox -c $BUILDDIR/bin/cmake -S $SOURCE
```

## Windows

First, build and test this program to create `./build/no-children.exe`:

```sh
cmake --workflow default
```

We use built-in Windows methods to deny child process launch:

```sh
./cmake_sandbox.bat -c %BUILDDIR%/bin/cmake.exe -S %SOURCE%
```
