- Unix allowlist for child execs is in no-children_unix.c via allowed_execs[] and is_whitelisted_exec().
- Whitelisted names currently include make/ninja/cmake and common compiler+linker tools.
- Allowed exec/spawn calls are forwarded to real libc symbols via dlsym(RTLD_NEXT, ...); others return EACCES.
- fork/vfork are allowed only when current process basename is whitelisted.

- On macOS, reliable interposition uses DYLD_INTERPOSE entries; plain symbol overrides may not catch fork/posix_spawnp.
- posix_spawn/posix_spawnp must return EACCES (error code), not -1, when blocked.

- Added nochild-selftest.c test target to validate interposer activity via blocked fork + posix_spawnp checks.

- On macOS with Xcode 16 SDK and -Werror, references to vfork in no-children_unix.c require local compiler diagnostics suppression for -Wdeprecated-declarations (use #pragma GCC diagnostic so it works with both Clang and Homebrew GCC).
- macOS DYLD_SIP_CONTROL is environment-dependent: some GitHub Actions runners allow DYLD_INSERT_LIBRARIES into /usr/bin/make, so the test should skip rather than fail when that binary is not a usable SIP negative control.
