// nochild.cpp - Windows version: blocks most child processes but allows whitelisted tools
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

bool IsWhitelisted(const wchar_t* path)
{
    if (!path) return false;

    // Match only the executable basename to avoid false positives like "cmake" matching "make".
    const wchar_t* basename = path;
    for (const wchar_t* p = path; *p != L'\0'; ++p) {
        if (*p == L'\\' || *p == L'/') {
            basename = p + 1;
        }
    }

    wchar_t lower[MAX_PATH];
    wcscpy_s(lower, basename);
    _wcslwr(lower);

    const wchar_t* allowed[] = {
        L"ninja", L"ninja.exe", L"make", L"make.exe", L"nmake", L"nmake.exe", L"msbuild", L"msbuild.exe",
        L"cc", L"cc.exe", L"gcc", L"gcc.exe", L"clang", L"clang.exe", L"cl", L"cl.exe",
        L"link", L"link.exe", L"ld", L"ld.exe", L"ar", L"ar.exe", L"ranlib", L"ranlib.exe",
        L"dlltool", L"dlltool.exe", L"objcopy", L"objcopy.exe",
        NULL
    };

    for (int i = 0; allowed[i] != NULL; ++i) {
        if (wcscmp(lower, allowed[i]) == 0) {
            return true;
        }
    }
    return false;
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2) {
        wprintf(L"Usage: nochild.exe <command> [args...]\n");
        return 1;
    }

    const wchar_t* cmd = argv[1];

    // If the command itself is whitelisted (ninja, cl, etc.), run it without restrictions
    if (IsWhitelisted(cmd)) {
        wprintf(L"[nochild] Whitelisted tool detected → running without restrictions: %ls\n", cmd);
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        wchar_t cmdline[32768] = {0};

        for (int i = 1; i < argc; ++i) {
            wcscat_s(cmdline, _countof(cmdline), L"\"");
            wcscat_s(cmdline, _countof(cmdline), argv[i]);
            wcscat_s(cmdline, _countof(cmdline), L"\" ");
        }

        if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            wprintf(L"CreateProcess failed: %lu\n", GetLastError());
            return 1;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode;
    }

    // === For everything else (especially CMake), apply strict limit ===
    wprintf(L"[nochild] Applying child process restriction to: %ls\n", cmd);

    HANDLE hJob = CreateJobObject(NULL, NULL);
    if (!hJob) {
        wprintf(L"CreateJobObject failed\n");
        return 1;
    }

    JOBOBJECT_BASIC_LIMIT_INFORMATION limits = { 0 };
    limits.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    limits.ActiveProcessLimit = 2;           // cmake + one active grandchild at a time (nochild.exe is outside the job)

    if (!SetInformationJobObject(hJob, JobObjectBasicLimitInformation, &limits, sizeof(limits))) {
        wprintf(L"SetInformationJobObject failed\n");
        CloseHandle(hJob);
        return 1;
    }

    // Launch the main command suspended so we can assign it (not ourselves) to the job.
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    wchar_t cmdline[32768] = {0};

    for (int i = 1; i < argc; ++i) {
        wcscat_s(cmdline, _countof(cmdline), L"\"");
        wcscat_s(cmdline, _countof(cmdline), argv[i]);
        wcscat_s(cmdline, _countof(cmdline), L"\" ");
    }

    if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        wprintf(L"CreateProcess failed: %lu\n", GetLastError());
        CloseHandle(hJob);
        return 1;
    }

    // Assign only the child (not this wrapper) to the job, then let it run.
    if (!AssignProcessToJobObject(hJob, pi.hProcess)) {
        wprintf(L"AssignProcessToJobObject failed: %lu\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hJob);
        return 1;
    }
    ResumeThread(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hJob);

    return exitCode;
}
