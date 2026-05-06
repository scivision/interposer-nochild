# Interposer to deny child process launch

When testing a program like CMake for internal development, it is useful to deny the program the ablility to execute child processes to help enusre the top program is stable in such cases.
This technique is demonstrated for Linux, macOS, and Windows.
This is not a cybersecurity sandbox, but rather a limited development tool to test stability of the top program when child process launch fails.
For actual
[sandboxing](https://hardenedlinux.org/blog/2024-08-20-gnu/linux-sandboxing-a-brief-review/),
instead use the off-the-shelf sandboxing tools for the platform that also test programs with denied access to filesystem, network, child processes, etc. such as
[Linux Firejail](https://github.com/netblue30/firejail),
[macOS App Sandbox](https://developer.apple.com/documentation/xcode/configuring-the-macos-app-sandbox),
or
[Windows Sandbox](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/).

We assume the CMake code under test has already been built like

```sh
cmake -S /path/to/cmake_source -B /tmp/build
cmake --build /tmp/build
```

Specify the path to this CMake executable using the scripts below with option like `-c /tmp/build/bin/cmake`

## Linux

Similar to macOS, a dynamic library with `LD_PRELOAD` can be used to interpose the `execve` system call and deny child process launch.

Separately, the [seccomp](./seccomp) directory shows use of Linux seccomp to deny child process launch.

## macOS

`sandbox-exec` can lockup macOS if there is an infinite loop in the program when child process launch is denied.
One can use this interposer dylib instead on macOS.

Sandbox-exec: simpler, but may lockup macOS requiring hard reboot.

```sh
./cmake_sandbox.sh sandbox -c /path/to/dev/cmake
```

Or, build interposer no-children.dylib

```sh
make
```

Use interposer

```sh
./cmake_sandbox.sh dylib -c /path/to/dev/cmake
```

## Windows

We use built-in Windows methods to deny child process launch.

```sh
mingw32-make
```

Then run the sandbox

```sh
./cmake_sandbox.bat -c /path/to/dev/cmake.exe
```
