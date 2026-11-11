// ChildProcess (child_process.h) on Windows, with CreateProcessW: the programs the launcher starts
// are console programs, and SDL's processes would open a console window for each one (it only
// leaves the window out for processes whose exit code it then drops).

#include "child_process.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <map>

namespace {

std::wstring wide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

std::string last_error_text(const std::string& what) {
    return what + " failed (Windows error " + std::to_string(GetLastError()) + ")";
}

// One argument as the C runtime of the program will split it back (the rules of
// CommandLineToArgvW): quoted when it has spaces or quotes, with the backslashes before a quote
// doubled.
void append_argument(std::wstring& command_line, const std::wstring& argument) {
    if (!command_line.empty()) {
        command_line += L' ';
    }
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        command_line += argument;
        return;
    }
    command_line += L'"';
    size_t backslashes = 0;
    for (const wchar_t letter : argument) {
        if (letter == L'\\') {
            ++backslashes;
            continue;
        }
        if (letter == L'"') {
            command_line.append(backslashes * 2 + 1, L'\\');
        } else {
            command_line.append(backslashes, L'\\');
        }
        backslashes = 0;
        command_line += letter;
    }
    command_line.append(backslashes * 2, L'\\');
    command_line += L'"';
}

// The launcher's environment with `extra` added (replacing variables of the same name), as the
// block CreateProcessW takes: NAME=value\0...\0\0.
std::wstring environment_block(const std::vector<std::pair<std::string, std::string>>& extra) {
    struct CaseInsensitive {
        bool operator()(const std::wstring& a, const std::wstring& b) const {
            return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()),
                                        TRUE) == CSTR_LESS_THAN;
        }
    };
    std::map<std::wstring, std::wstring, CaseInsensitive> variables;
    if (wchar_t* block = GetEnvironmentStringsW(); block != nullptr) {
        for (const wchar_t* entry = block; *entry != L'\0'; entry += wcslen(entry) + 1) {
            const std::wstring text(entry);
            // Entries that start with "=" are the current folders of the drives (=C:=C:\...).
            const size_t equals = text.find(L'=', 1);
            if (equals != std::wstring::npos) {
                variables[text.substr(0, equals)] = text.substr(equals + 1);
            }
        }
        FreeEnvironmentStringsW(block);
    }
    for (const auto& [name, value] : extra) {
        variables[wide(name)] = wide(value);
    }
    std::wstring result;
    for (const auto& [name, value] : variables) {
        result += name + L'=' + value + L'\0';
    }
    result += L'\0';
    return result;
}

class WindowsChildProcess final : public ChildProcess {
public:
    WindowsChildProcess(HANDLE process, HANDLE job, HANDLE output) : process_(process), job_(job), output_(output) {}

    ~WindowsChildProcess() override {
        if (output_ != nullptr) {
            CloseHandle(output_);
        }
        if (job_ != nullptr) {
            CloseHandle(job_);  // ends whatever of it still runs
        }
        CloseHandle(process_);
    }

    std::string read_output() override {
        if (output_ == nullptr) {
            return {};
        }
        char buffer[4096];
        DWORD read = 0;
        if (!ReadFile(output_, buffer, sizeof(buffer), &read, nullptr)) {
            return {};  // the program closed its end of the pipe
        }
        return std::string(buffer, read);
    }

    std::optional<int> exit_code() override {
        if (WaitForSingleObject(process_, 0) != WAIT_OBJECT_0) {
            return std::nullopt;
        }
        DWORD code = 0;
        GetExitCodeProcess(process_, &code);
        return static_cast<int>(code);
    }

    void kill() override {
        if (job_ != nullptr) {
            TerminateJobObject(job_, 1);
        } else {
            TerminateProcess(process_, 1);
        }
    }

private:
    HANDLE process_;
    HANDLE job_;
    HANDLE output_;  // the launcher's end of the output pipe, or null
};

}  // namespace

std::unique_ptr<ChildProcess> ChildProcess::start(const Options& options, std::string& error) {
    SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE child_output = nullptr;
    HANDLE launcher_output = nullptr;
    if (options.output_file.empty()) {
        if (!CreatePipe(&launcher_output, &child_output, &inheritable, 0)) {
            error = last_error_text("CreatePipe");
            CloseHandle(input);
            return nullptr;
        }
        SetHandleInformation(launcher_output, HANDLE_FLAG_INHERIT, 0);  // the child only gets its end
    } else {
        child_output = CreateFileW(wide(options.output_file).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &inheritable, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (child_output == INVALID_HANDLE_VALUE) {
            error = last_error_text("CreateFileW (" + options.output_file + ")");
            CloseHandle(input);
            return nullptr;
        }
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = child_output;
    startup.hStdError = child_output;

    std::wstring command_line;
    for (const std::string& argument : options.arguments) {
        append_argument(command_line, wide(argument));
    }
    std::wstring environment = environment_block(options.environment);
    const std::wstring working_directory = wide(options.working_directory);

    HANDLE job = nullptr;
    DWORD flags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;
    if (options.end_with_launcher) {
        job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        flags |= CREATE_SUSPENDED;  // in the job before it can start anything
    }
    PROCESS_INFORMATION information{};
    const BOOL created = CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, TRUE, flags,
                                        environment.data(),
                                        working_directory.empty() ? nullptr : working_directory.c_str(), &startup,
                                        &information);
    CloseHandle(input);
    CloseHandle(child_output);  // the child has its own copy
    if (!created) {
        error = last_error_text("CreateProcessW");
        if (launcher_output != nullptr) {
            CloseHandle(launcher_output);
        }
        if (job != nullptr) {
            CloseHandle(job);
        }
        return nullptr;
    }
    if (job != nullptr) {
        AssignProcessToJobObject(job, information.hProcess);
        ResumeThread(information.hThread);
    }
    CloseHandle(information.hThread);
    return std::make_unique<WindowsChildProcess>(information.hProcess, job, launcher_output);
}
