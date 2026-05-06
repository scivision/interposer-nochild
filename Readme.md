# Interposer to deny child process launch

When testing a program like CMake for internal development, it is useful to deny the program the ablility to execute child processes to help enusre the top program is stable in such cases.
This technique is demonstrated for Linux, macOS, and Windows.

We assume the CMake code under test has already been built like

```sh
cmake -B build
cmake --build build
```

And that you execute `cmake_sandbox.{sh,bat}` from the root of the CMake source tree.

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
./cmake_sandbox.sh
```

## macOS

`sandbox-exec` can lockup macOS if there is an infinite loop in the program when child process launch is denied.
One can use this interposer dylib instead on macOS.

Sandbox-exec: simpler, but may lockup macOS requiring hard reboot.

```sh
./cmake_sandbox.sh sandbox
```

Or, build interposer no-children.dylib

```sh
make
```

Use interposer

```sh
./cmake_sandbox.sh dylib
```

## Windows

We use built-in Windows methods to deny child process launch.

```sh
mingw32-make
```

Then run the sandbox

```sh
./cmake_sandbox.bat
```
