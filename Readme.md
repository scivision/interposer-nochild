# Interposer dylib for macOS to deny child process launch

`sandbox-exec` can lockup macOS is there is an infinite loop in the program when child process launch is denied. One can use this interposer dylib instead on macOS.

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
