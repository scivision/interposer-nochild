# Interposer to deny child process launch

When testing a program like CMake for internal development, it is useful to deny the program the ablility to execute child processes to help enusre the top program is stable in such cases.
This technique is demonstrated for Linux, macOS, and Windows.

We assume the CMake code under test has already been built like

```sh
cmake -B build
cmake --build build
```

Specify the path to this CMake executable using the scripts below with option `-c /path/to/dev/cmake` e.g. `-c build/bin/cmake`

## Linux

Use `seccomp` and a small helper program.

```sh
apt install libseccomp-dev
```

```sh
make
```

Then run the sandbox

```sh
./cmake_sandbox.sh -c /path/to/dev/cmake
```

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
