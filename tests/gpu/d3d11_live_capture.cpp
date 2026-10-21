#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cwchar>
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

int wmain(int argc,wchar_t **argv) {
    const bool close_mode=argc==2 && std::wcscmp(argv[1],L"--close")==0;
    const bool m2c_mode=argc==3 && (std::wcscmp(argv[1],L"--m2c")==0 ||
                                       std::wcscmp(argv[1],L"--m2c-hardware")==0);
    const bool m2c_hardware=m2c_mode && std::wcscmp(argv[1],L"--m2c-hardware")==0;
    const bool m2d_mode=argc==4 && (std::wcscmp(argv[1],L"--m2d")==0 ||
                                       std::wcscmp(argv[1],L"--m2d-hardware")==0);
    const bool m2d_hardware=m2d_mode && std::wcscmp(argv[1],L"--m2d-hardware")==0;
    const bool ownership_mode=argc==4 && (std::wcscmp(argv[1],L"--ownership")==0 ||
                                         std::wcscmp(argv[1],L"--ownership-hardware")==0);
    const bool ownership_hardware=ownership_mode && std::wcscmp(argv[1],L"--ownership-hardware")==0;
    if (argc>1 && !close_mode && !m2c_mode && !m2d_mode && !ownership_mode) return 2;
    wchar_t self[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr,self,MAX_PATH)) return 2;
    std::wstring child=self;
    const auto slash=child.find_last_of(L"\\/");
    if (slash==std::wstring::npos) return 2;
    if (m2c_mode || m2d_mode || ownership_mode) {
        const auto parent=child.find_last_of(L"\\/",slash-1);
        if (parent==std::wstring::npos) return 2;
        const auto config=child.substr(parent+1,slash-parent-1);
        child.resize(parent+1);
        child+=L"bin\\"+config+(ownership_mode?L"\\ars_context_ownership_tests.exe":m2d_mode?L"\\ars_m2_dual_pixels.exe":
                                     L"\\ars_m2_3d_pixels.exe");
    } else {
        child.resize(slash+1);
        child+=close_mode ? L"ars_d3d11_close_physical.exe" : L"ars_d3d11_query_retirement.exe";
    }
    std::wstring command=L"\""+child+L"\"";
    if(ownership_mode)command+=L" \""+std::wstring(argv[2])+L"\" \""+std::wstring(argv[3])+L"\""+(ownership_hardware?L" --hardware":L"");
    if (m2c_mode) command+=L" \""+std::wstring(argv[2])+L"\" "+
                           (m2c_hardware?L"--live-hardware":L"--live-report");
    if (m2d_mode) command+=L" \""+std::wstring(argv[2])+L"\" \""+
                           std::wstring(argv[3])+L"\" "+
                           (m2d_hardware?L"--live-hardware":L"--live-report");
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
    std::puts(ownership_mode ? "PASS alpha38 owned context live-object identity unchanged" :
        m2d_mode ? "PASS M2-D dual-mode live-object identity unchanged" :
        m2c_mode ? "PASS M2-C healthy 3D live-object identity unchanged" :
        close_mode ? "PASS D07 physical post-close live-object identity unchanged across 100 cycles" :
        "PASS D06 physical debug-layer live-object identity unchanged across 100 retire/recreate cycles");
    return 0;
}
