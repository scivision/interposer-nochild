# Linux seccomp example

Linux
[seccomp](https://man7.org/linux/man-pages/man2/seccomp.2.html)
in a launcher program can restrict access to system calls.
We specifically disclaim any correctness of our example code, which is a demonstration of the technique.

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
