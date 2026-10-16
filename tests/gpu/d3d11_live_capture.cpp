#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

static std::set<std::string> live_objects(const std::string &report) {
    std::set<std::string> objects;
    std::size_t start=0;
    while (start<report.size()) {
        const auto end=report.find('\n',start);
        const auto line=report.substr(start,end==std::string::npos ? end : end-start);
        const auto live=line.find("Live ID3D11");
        const auto ref=line.find(", Refcount:",live==std::string::npos ? 0 : live);
        if (live!=std::string::npos && ref!=std::string::npos)
            objects.insert(line.substr(live,ref-live));
        if (end==std::string::npos) break;
        start=end+1;
    }
    return objects;
}

int wmain() {
    wchar_t self[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr,self,MAX_PATH)) return 2;
    std::wstring child=self;
    const auto slash=child.find_last_of(L"\\/");
    if (slash==std::wstring::npos) return 2;
    child.resize(slash+1);
    child+=L"ars_d3d11_query_retirement.exe";
    std::wstring command=L"\""+child+L"\"";
    STARTUPINFOW startup{};
    startup.cb=sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(child.c_str(),command.data(),nullptr,nullptr,FALSE,
                        DEBUG_ONLY_THIS_PROCESS, nullptr,nullptr,&startup,&process)) {
        std::printf("HOLD debugger child launch failed error=%lu\n",GetLastError());
        return 2;
    }
    std::string baseline, final_report;
    int phase=0;
    DWORD child_exit=1;
    bool exited=false;
    while (!exited) {
        DEBUG_EVENT event{};
        if (!WaitForDebugEvent(&event,30000)) { std::puts("HOLD debug event timeout"); break; }
        DWORD continuation=DBG_CONTINUE;
        if (event.dwDebugEventCode==OUTPUT_DEBUG_STRING_EVENT) {
            const auto info=event.u.DebugString;
            const SIZE_T bytes=std::min<SIZE_T>(info.nDebugStringLength,8192) *
                (info.fUnicode?sizeof(wchar_t):sizeof(char));
            std::vector<char> buffer(bytes+sizeof(wchar_t));
            SIZE_T read=0;
            if (bytes && ReadProcessMemory(process.hProcess,info.lpDebugStringData,buffer.data(),bytes,&read)) {
                std::string message;
                if (info.fUnicode) {
                    const auto *wide=reinterpret_cast<const wchar_t *>(buffer.data());
                    const int length=static_cast<int>(read/sizeof(wchar_t));
                    const int needed=WideCharToMultiByte(CP_UTF8,0,wide,length,nullptr,0,nullptr,nullptr);
                    if (needed>0) {
                        message.resize(static_cast<std::size_t>(needed));
                        WideCharToMultiByte(CP_UTF8,0,wide,length,message.data(),needed,nullptr,nullptr);
                    }
                } else message.assign(buffer.data(),read);
                message.erase(std::find(message.begin(),message.end(),'\0'),message.end());
                if (message.find("ARS_LIVE_BASELINE")!=std::string::npos) phase=1;
                else if (message.find("ARS_LIVE_FINAL")!=std::string::npos) phase=2;
                else if (phase==1) baseline+=message;
                else if (phase==2) final_report+=message;
            }
        } else if (event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT) {
            if (event.u.CreateProcessInfo.hFile) CloseHandle(event.u.CreateProcessInfo.hFile);
        } else if (event.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT) {
            if (event.u.LoadDll.hFile) CloseHandle(event.u.LoadDll.hFile);
        } else if (event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT &&
                   event.u.Exception.ExceptionRecord.ExceptionCode!=EXCEPTION_BREAKPOINT) {
            continuation=DBG_EXCEPTION_NOT_HANDLED;
        } else if (event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT) {
            child_exit=event.u.ExitProcess.dwExitCode;
            exited=true;
        }
        ContinueDebugEvent(event.dwProcessId,event.dwThreadId,continuation);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    std::printf("child_exit=%lu baseline_bytes=%zu final_bytes=%zu\n",child_exit,baseline.size(),final_report.size());
    std::printf("BASELINE_DEBUG_REPORT\n%s\nFINAL_DEBUG_REPORT\n%s\n",baseline.c_str(),final_report.c_str());
    if (child_exit!=0 || baseline.empty() || final_report.empty()) return 3;
    const auto before=live_objects(baseline);
    const auto after=live_objects(final_report);
    std::printf("live_objects_before=%zu live_objects_after=%zu\n",before.size(),after.size());
    if (before.empty() || before!=after) return 4;
    std::puts("PASS D06 physical debug-layer live-object identity unchanged across 100 retire/recreate cycles");
    return 0;
}
