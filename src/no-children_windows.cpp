// nochild.cpp - Windows version: blocks most child processes but allows whitelisted tools

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ostream> // for std::endl

#include <string>
#include <system_error>
#include <string_view>
#include <filesystem>
#include <array>
#include <iostream>
#include <algorithm>


bool IsWhitelisted(std::string_view path)
{
    if (path.empty())
      return false;

    // Match only the executable basename to avoid false positives like "cmake" matching "make".
    auto basename = std::filesystem::path(path).filename().string();

    auto to_lower = [](char c) -> char {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };
    std::transform(basename.begin(), basename.end(), basename.begin(), to_lower);

    std::array<std::string_view, 28> allowed = {
        "ninja", "ninja.exe", "make", "make.exe", "nmake", "nmake.exe", "msbuild", "msbuild.exe",
        "cc", "cc.exe", "gcc", "gcc.exe", "clang", "clang.exe", "cl", "cl.exe",
        "link", "link.exe", "ld", "ld.exe", "ar", "ar.exe", "ranlib", "ranlib.exe",
        "dlltool", "dlltool.exe", "objcopy", "objcopy.exe"
    };

    return std::find(allowed.begin(), allowed.end(), std::string_view{basename}) != allowed.end();
}

static std::string BuildQuotedCommandLine(int argc, char* argv[])
{
    std::string cmdline;
    for (int i = 1; i < argc; ++i) {
        cmdline.push_back('"');
        cmdline += argv[i];
        cmdline.push_back('"');
        if (i + 1 < argc) {
            cmdline.push_back(' ');
        }
    }
    return cmdline;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "Usage: nochild.exe <command> [args...]" << std::endl;
        return EXIT_FAILURE;
    }

    std::string_view cmd = argv[1];

    // If the command itself is whitelisted (ninja, cl, etc.), run it without restrictions
    if (IsWhitelisted(cmd)) {
        std::cout << "[nochild] Whitelisted tool detected → running without restrictions: " << cmd << std::endl;
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        std::string cmdline = BuildQuotedCommandLine(argc, argv);

        // CreateProcess may modify the command-line buffer, so keep it mutable.
        if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            std::cerr << "CreateProcess failed: " << std::system_category().message(GetLastError()) << std::endl;
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
    std::cout << "[nochild] Applying child process restriction to: " << cmd << std::endl;

    HANDLE hJob = CreateJobObject(nullptr, nullptr);
    if (!hJob) {
        std::cerr << "CreateJobObject failed: " << std::system_category().message(GetLastError()) << std::endl;
        return EXIT_FAILURE;
    }

    JOBOBJECT_BASIC_LIMIT_INFORMATION limits = { 0 };
    limits.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    limits.ActiveProcessLimit = 2;           // cmake + one active grandchild at a time (nochild.exe is outside the job)

    if (!SetInformationJobObject(hJob, JobObjectBasicLimitInformation, &limits, sizeof(limits))) {
        std::cerr << "SetInformationJobObject failed: " << std::system_category().message(GetLastError()) << std::endl;
        CloseHandle(hJob);
        return EXIT_FAILURE;
    }

    // Launch the main command suspended so we can assign it (not ourselves) to the job.
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    std::string cmdline = BuildQuotedCommandLine(argc, argv);

    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
        std::cerr << "CreateProcess failed: " << std::system_category().message(GetLastError()) << std::endl;
        CloseHandle(hJob);
        return EXIT_FAILURE;
    }

    // Assign only the child (not this wrapper) to the job, then let it run.
    if (!AssignProcessToJobObject(hJob, pi.hProcess)) {
        std::cerr << "AssignProcessToJobObject failed: " << std::system_category().message(GetLastError()) << std::endl;
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hJob);
        return EXIT_FAILURE;
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
