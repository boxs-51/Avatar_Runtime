#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "schema_worker_client.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include <windows.h>
namespace ars::validation {
namespace {
struct Handle {
    HANDLE h = nullptr;
    ~Handle() {
        if (h && h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
    }
    Handle() = default;
    Handle(const Handle &) = delete;
};
struct Io {
    HANDLE input = nullptr, output = nullptr;
    const Message *header = nullptr;
    std::string_view body;
    std::array<std::uint8_t, 33> reply{};
    std::size_t received = 0;
    bool ok = false;
};
DWORD WINAPI communicate(void *p) {
    auto &io = *static_cast<Io *>(p);
    auto write = [&](const void *bytes, std::size_t count) {
        auto *ptr = static_cast<const std::uint8_t *>(bytes);
        while (count) {
            DWORD n = 0;
            const auto wanted = static_cast<DWORD>(std::min<std::size_t>(count, 65536));
            if (!WriteFile(io.input, ptr, wanted, &n, nullptr) || !n)
                return false;
            ptr += n;
            count -= n;
        }
        return true;
    };
    bool written = write(io.header->data(), 32) && write(io.body.data(), io.body.size());
    CloseHandle(io.input);
    io.input = nullptr;
    if (!written)
        return 0;
    while (io.received < io.reply.size()) {
        DWORD n = 0;
        if (!ReadFile(io.output, io.reply.data() + io.received,
                      static_cast<DWORD>(io.reply.size() - io.received), &n, nullptr)) {
            io.ok = GetLastError() == ERROR_BROKEN_PIPE && io.received == 32;
            return 0;
        }
        if (!n) {
            io.ok = io.received == 32;
            return 0;
        }
        io.received += n;
    }
    return 0;
}
} // namespace
WorkerResult run_schema_process(const WorkerConfig &config, const Message &request,
                                std::string_view input) noexcept {
    WorkerResult result;
    try {
        const auto started = GetTickCount64();
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        Handle read_in, write_in, read_out, write_out, error, job, process, thread, io_thread;
        if (!CreatePipe(&read_in.h, &write_in.h, &sa, 65536) ||
            !CreatePipe(&read_out.h, &write_out.h, &sa, 65536) ||
            !SetHandleInformation(write_in.h, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(read_out.h, HANDLE_FLAG_INHERIT, 0))
            return result;
        error.h = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING,
                              0, nullptr);
        if (error.h == INVALID_HANDLE_VALUE)
            return result;
        job.h = CreateJobObjectW(nullptr, nullptr);
        if (!job.h)
            return result;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
        limit.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_PROCESS_TIME;
        limit.BasicLimitInformation.ActiveProcessLimit = 1;
        limit.ProcessMemoryLimit = worker_memory;
        limit.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart = 50000000;
        if (!SetInformationJobObject(job.h, JobObjectExtendedLimitInformation, &limit, sizeof(limit)))
            return result;
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        // Fixed path <=32760 and fixed attribute count, charged in the IPC reservation.
        std::vector<std::uint8_t> attributes(bytes);
        auto *attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        if (!InitializeProcThreadAttributeList(attrs, 1, 0, &bytes))
            return result;
        struct AttrGuard {
            LPPROC_THREAD_ATTRIBUTE_LIST p;
            ~AttrGuard() { DeleteProcThreadAttributeList(p); }
        } attr_guard{attrs};
        HANDLE inherited[]{read_in.h, write_out.h, error.h};
        if (!UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
                                       sizeof(inherited), nullptr, nullptr))
            return result;
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, config.executable.data(),
                                              static_cast<int>(config.executable.size()), nullptr, 0);
        if (!count)
            return result;
        std::wstring path(static_cast<std::size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, config.executable.data(),
                            static_cast<int>(config.executable.size()), path.data(), count);
        std::wstring command = L"\"" + path + L"\"";
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = read_in.h;
        startup.StartupInfo.hStdOutput = write_out.h;
        startup.StartupInfo.hStdError = error.h;
        startup.lpAttributeList = attrs;
        PROCESS_INFORMATION info{};
        if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, TRUE,
                            CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr,
                            nullptr, &startup.StartupInfo, &info))
            return result;
        process.h = info.hProcess;
        thread.h = info.hThread;
        result.process_id = info.dwProcessId;
        auto terminate = [&] {
            TerminateProcess(process.h, 9);
            WaitForSingleObject(process.h, INFINITE);
            result.reaped = true;
        };
        if (!AssignProcessToJobObject(job.h, process.h)) {
            terminate();
            return result;
        }
        CloseHandle(read_in.h);
        read_in.h = nullptr;
        CloseHandle(write_out.h);
        write_out.h = nullptr;
        if (ResumeThread(thread.h) == DWORD(-1)) {
            terminate();
            return result;
        }
        Io io{write_in.h, read_out.h, &request, input};
        write_in.h = nullptr;
        io_thread.h = CreateThread(nullptr, 256 * 1024, communicate, &io, 0, nullptr);
        if (!io_thread.h) {
            CloseHandle(io.input);
            terminate();
            return result;
        }
        const auto elapsed = GetTickCount64() - started;
        const DWORD remaining =
            elapsed >= config.deadline_ms ? 0 : config.deadline_ms - static_cast<DWORD>(elapsed);
        const auto done = WaitForSingleObject(io_thread.h, remaining);
        if (done != WAIT_OBJECT_0) {
            TerminateJobObject(job.h, 9);
            WaitForSingleObject(process.h, INFINITE);
            WaitForSingleObject(io_thread.h, INFINITE);
            result.reaped = true;
            result.status = ARS_E_OUT_OF_BUDGET;
            return result;
        }
        const auto elapsed2 = GetTickCount64() - started;
        const DWORD remaining2 =
            elapsed2 >= config.deadline_ms ? 0 : config.deadline_ms - static_cast<DWORD>(elapsed2);
        if (WaitForSingleObject(process.h, remaining2) != WAIT_OBJECT_0) {
            terminate();
            result.status = ARS_E_OUT_OF_BUDGET;
            return result;
        }
        result.reaped = true;
        DWORD code = 1;
        GetExitCodeProcess(process.h, &code);
        if (io.ok && code == 0 && io.received == 32) {
            Message reply{};
            std::copy_n(io.reply.begin(), 32, reply.begin());
            if (valid_reply(reply, request))
                result.status = static_cast<ars_status>(get(reply, 8));
        }
        return result;
    } catch (...) {
        return result;
    }
}
} // namespace ars::validation
