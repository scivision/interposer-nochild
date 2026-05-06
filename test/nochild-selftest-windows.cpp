// nochild-selftest-windows.cpp - Test that verifies Windows job object child process blocking

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <system_error>

static int expect_blocked_createprocess(void)
{
    // Get full path to cmd.exe via SearchPath or construct from SYSTEMROOT
    char cmd_path[MAX_PATH] = {0};
    if (!SearchPathA(nullptr, "cmd.exe", nullptr, MAX_PATH, cmd_path, nullptr)) {
        // Fallback: try constructing from SYSTEMROOT
        char sysroot[MAX_PATH] = {0};
        DWORD len = GetEnvironmentVariableA("SYSTEMROOT", sysroot, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) {
            std::cerr << "[selftest] FAIL: Could not locate cmd.exe\n";
            return 1;
        }
        snprintf(cmd_path, MAX_PATH, "%s\\System32\\cmd.exe", sysroot);
    }

    // Try to spawn cmd.exe twice - first should succeed (within job limit), second should fail
    STARTUPINFOA si = { sizeof(si) };
    char cmdline[MAX_PATH + 16] = {0};
    snprintf(cmdline, sizeof(cmdline), "\"%s\" /c timeout /t 5", cmd_path);

    // First child process - should succeed (within limit of 2: test + child1)
    PROCESS_INFORMATION pi1 = { 0 };
    if (!CreateProcessA(nullptr, cmdline, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi1)) {
        std::cerr << "[selftest] FAIL: First CreateProcess failed unexpectedly: " << GetLastError() << "\n";
        return 1;
    }
    std::cerr << "[selftest] First child process created (pid=" << pi1.dwProcessId << ")\n";

    // Second child process - should fail (would exceed limit of 2)
    PROCESS_INFORMATION pi2 = { 0 };
    if (CreateProcessA(nullptr, cmdline, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi2)) {
        // Second process was created successfully - this means we're NOT constrained
        WaitForSingleObject(pi1.hProcess, INFINITE);
        WaitForSingleObject(pi2.hProcess, INFINITE);
        CloseHandle(pi1.hProcess);
        CloseHandle(pi1.hThread);
        CloseHandle(pi2.hProcess);
        CloseHandle(pi2.hThread);

        std::cerr << "[selftest] FAIL: Second CreateProcess succeeded when it should be blocked\n";
        return 1;
    }

    const DWORD error = GetLastError();

    // Terminate first child to clean up
    TerminateProcess(pi1.hProcess, 0);
    WaitForSingleObject(pi1.hProcess, INFINITE);
    CloseHandle(pi1.hProcess);
    CloseHandle(pi1.hThread);

    // When a job object limit is hit, we typically get ERROR_NOT_ENOUGH_QUOTA (1816)
    // or ERROR_INVALID_HANDLE in some contexts
    if (error == ERROR_NOT_ENOUGH_QUOTA) {
        std::cerr << "[selftest] child process creation blocked as expected (ERROR_NOT_ENOUGH_QUOTA)\n";
        return 0;
    }

    // Alternative: some contexts might give us ERROR_ACCESS_DENIED (5)
    if (error == ERROR_ACCESS_DENIED) {
        std::cerr << "[selftest] child process creation blocked as expected (ERROR_ACCESS_DENIED)\n";
        return 0;
    }

    std::cerr << "[selftest] FAIL: CreateProcess failed with unexpected error "
              << error << " (" << std::system_category().message(error)
              << ")\n";
    return 1;
}

int main(void)
{
    int failures = 0;

    failures += expect_blocked_createprocess();

    if (failures == 0) {
        std::cerr << "[selftest] PASS: interposer is active and child launch is blocked.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[selftest] FAIL: interposer not active for this process.\n";
    return EXIT_FAILURE;
}
