#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <userenv.h>
#include <aclapi.h>
#include <sddl.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <io.h>

struct Handle{
    HANDLE value=nullptr;
    ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    Handle()=default;Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
struct Profile{
    std::wstring name;PSID sid=nullptr;bool created=false;
    ~Profile(){if(sid)FreeSid(sid);if(created)DeleteAppContainerProfile(name.c_str());}
};
bool grant_read(const std::filesystem::path& path,PSID sid){
    Handle token;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value))return false;
    DWORD size=0;GetTokenInformation(token.value,TokenUser,nullptr,0,&size);std::vector<unsigned char> user(size);
    if(!GetTokenInformation(token.value,TokenUser,user.data(),size,&size))return false;
    alignas(SID) unsigned char system[SECURITY_MAX_SID_SIZE]{};DWORD sid_size=sizeof(system);
    if(!CreateWellKnownSid(WinLocalSystemSid,nullptr,system,&sid_size))return false;
    EXPLICIT_ACCESSW access[3]{};PSID principals[]{reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid,system,sid};
    for(unsigned i=0;i<3;++i){
        access[i].grfAccessPermissions=i==2?FILE_GENERIC_READ|FILE_GENERIC_EXECUTE:FILE_ALL_ACCESS;
        access[i].grfAccessMode=GRANT_ACCESS;access[i].grfInheritance=NO_INHERITANCE;
        access[i].Trustee.TrusteeForm=TRUSTEE_IS_SID;access[i].Trustee.TrusteeType=TRUSTEE_IS_UNKNOWN;
        access[i].Trustee.ptstrName=static_cast<LPWSTR>(principals[i]);
    }
    PACL acl=nullptr;const bool ok=SetEntriesInAclW(3,access,nullptr,&acl)==ERROR_SUCCESS&&
        SetNamedSecurityInfoW(const_cast<wchar_t*>(path.c_str()),SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,nullptr,nullptr,acl,nullptr)==ERROR_SUCCESS;
    if(acl)LocalFree(acl);return ok;
}
std::wstring quote(const std::wstring& value){
    std::wstring out=L"\"";std::size_t slashes=0;
    for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}out.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;if(c==L'"')out+=L'\\';out+=c;}
    out.append(slashes*2,L'\\');return out+L"\"";
}

int wmain(int argc,wchar_t** argv){
    if(argc<4)return 2;
    std::size_t output_cap=64;
#ifdef ARS_MOC3_PACKAGE_ADMISSION
    const bool package=std::wstring(argv[3])==L"--package";
    if(package)output_cap=192ull*1024*1024;
#else
    const bool package=false;
#endif
    // The trusted broker chooses the worker copy and fresh profile name. Public bytes are stdin only.
    const auto worker=std::filesystem::absolute(argv[1]);
    if(!std::filesystem::is_regular_file(worker)||std::wstring(argv[2]).rfind(L"AvatarMocImport-",0)!=0)return 2;
    Profile profile;profile.name=argv[2];
    if(FAILED(CreateAppContainerProfile(profile.name.c_str(),L"Avatar MOC import",L"Temporary isolated parser",nullptr,0,&profile.sid)))return 3;
    profile.created=true;
    if(!grant_read(worker.parent_path(),profile.sid)||!grant_read(worker,profile.sid)||
       !grant_read(worker.parent_path()/"worker-data.txt",profile.sid))return 4;
    Handle job;job.value=CreateJobObjectW(nullptr,nullptr);if(!job.value)return 5;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_ACTIVE_PROCESS|
        JOB_OBJECT_LIMIT_PROCESS_MEMORY|JOB_OBJECT_LIMIT_JOB_MEMORY|JOB_OBJECT_LIMIT_PROCESS_TIME|JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    limits.BasicLimitInformation.ActiveProcessLimit=1;limits.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart=2*10000000ll;
    limits.ProcessMemoryLimit=512ull*1024*1024;limits.JobMemoryLimit=512ull*1024*1024;
    if(!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))return 6;
    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};Handle output_read,output_write,null_error;
    if(!CreatePipe(&output_read.value,&output_write.value,&inherit,0)||!SetHandleInformation(output_read.value,HANDLE_FLAG_INHERIT,0))return 7;
    HANDLE input=GetStdHandle(STD_INPUT_HANDLE);
    if(!input||input==INVALID_HANDLE_VALUE||!SetHandleInformation(input,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT))return 8;
    SECURITY_CAPABILITIES capabilities{};capabilities.AppContainerSid=profile.sid;
    null_error.value=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&inherit,OPEN_EXISTING,0,nullptr);
    if(null_error.value==INVALID_HANDLE_VALUE)return 8;
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);
    startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput=input;startup.StartupInfo.hStdOutput=output_write.value;startup.StartupInfo.hStdError=null_error.value;
    SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,3,0,&size);std::vector<unsigned char> storage(size);
    startup.lpAttributeList=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if(!InitializeProcThreadAttributeList(startup.lpAttributeList,3,0,&size))return 9;
    struct Attr{PPROC_THREAD_ATTRIBUTE_LIST value;~Attr(){DeleteProcThreadAttributeList(value);}} attributes{startup.lpAttributeList};
    HANDLE handles[]{input,output_write.value,null_error.value};DWORD child_policy=PROCESS_CREATION_CHILD_PROCESS_RESTRICTED;
    if(!UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,&capabilities,sizeof(capabilities),nullptr,nullptr)||
       !UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),nullptr,nullptr)||
       !UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY,&child_policy,sizeof(child_policy),nullptr,nullptr))return 10;
    std::wstring command=quote(worker.wstring());for(int i=3;i<argc;++i)command+=L" "+quote(argv[i]);
    wchar_t windows[MAX_PATH]{},system[MAX_PATH]{},local_appdata[32768]{};
    if(!GetWindowsDirectoryW(windows,MAX_PATH)||!GetSystemDirectoryW(system,MAX_PATH))return 10;
    const auto local_size=GetEnvironmentVariableW(L"LOCALAPPDATA",local_appdata,32768);
    if(!local_size||local_size>=32768)return 10;
    std::wstring environment;
    for(const auto& entry:std::vector<std::wstring>{L"LOCALAPPDATA="+std::wstring(local_appdata),L"PATH="+std::wstring(system),
                                                  L"SystemRoot="+std::wstring(windows),L"WINDIR="+std::wstring(windows)}){
        environment+=entry;environment.push_back(L'\0');
    }
    environment.push_back(L'\0');
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(worker.c_str(),command.data(),nullptr,nullptr,TRUE,
        EXTENDED_STARTUPINFO_PRESENT|CREATE_SUSPENDED|CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT,
        environment.data(),worker.parent_path().c_str(),&startup.StartupInfo,&process)){
        const auto error=GetLastError();std::ofstream failed(worker.parent_path()/"setup-error.json");
        failed<<"{\"win32_error\":"<<error<<"}";return 11;
    }
    Handle child,thread;child.value=process.hProcess;thread.value=process.hThread;
    if(!AssignProcessToJobObject(job.value,child.value)){TerminateProcess(child.value,1);WaitForSingleObject(child.value,5000);return 12;}
    CloseHandle(output_write.value);output_write.value=nullptr;
    std::string output;std::atomic<bool> overflow=false;
    std::thread reader([&]{
        char chunk[16384];DWORD count=0;
        while(ReadFile(output_read.value,chunk,sizeof(chunk),&count,nullptr)&&count){
            if(count>output_cap-output.size()){overflow=true;TerminateJobObject(job.value,1);break;}
            output.append(chunk,count);
        }
    });
    if(ResumeThread(thread.value)==static_cast<DWORD>(-1)){TerminateJobObject(job.value,1);}
    const auto began=GetTickCount64();DWORD wait=WAIT_TIMEOUT;bool cpu_rejected=false;
    while((wait=WaitForSingleObject(child.value,10))==WAIT_TIMEOUT){
        FILETIME created{},ended{},kernel_cpu{},user_cpu{};ULARGE_INTEGER measured{};
        if(!GetProcessTimes(child.value,&created,&ended,&kernel_cpu,&user_cpu)){TerminateJobObject(job.value,1);break;}
        measured.LowPart=user_cpu.dwLowDateTime;measured.HighPart=user_cpu.dwHighDateTime;
        if(measured.QuadPart>=20000000ull){cpu_rejected=true;TerminateJobObject(job.value,1);break;}
        if(GetTickCount64()-began>=5000){TerminateJobObject(job.value,1);break;}
    }
    if(wait!=WAIT_OBJECT_0)TerminateJobObject(job.value,1);
    WaitForSingleObject(child.value,5000);reader.join();
    DWORD code=1;GetExitCodeProcess(child.value,&code);
    FILETIME creation{},exit{},kernel_time{},user_time{};ULARGE_INTEGER cpu{};
    GetProcessTimes(child.value,&creation,&exit,&kernel_time,&user_time);
    cpu.LowPart=user_time.dwLowDateTime;cpu.HighPart=user_time.dwHighDateTime;
    std::ofstream diagnostic(worker.parent_path()/"launcher-state.json");
    diagnostic<<"{\"worker_exit\":"<<code<<",\"wait_result\":"<<wait<<",\"output_overflow\":"<<(overflow?"true":"false")
              <<",\"worker_user_cpu_100ns\":"<<cpu.QuadPart<<"}";
    diagnostic.close();
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
    if(!QueryInformationJobObject(job.value,JobObjectBasicAccountingInformation,&accounting,sizeof(accounting),nullptr)||accounting.ActiveProcesses!=0)return 13;
    // Clean up the profile explicitly and report cleanup failures; destructor covers earlier failures.
    if(FAILED(DeleteAppContainerProfile(profile.name.c_str())))return 14;
    profile.created=false;
    if(overflow){std::cout<<"OUTPUT_REJECTED"<<std::endl;return 0;}
    if(cpu_rejected){std::cout<<"CPU_REJECTED"<<std::endl;return 0;}
    if(wait!=WAIT_OBJECT_0){std::cout<<"TIMEOUT_REJECTED"<<std::endl;return 0;}
    if(code!=0){std::cout<<"WORKER_REJECTED"<<std::endl;return 0;}
    if(package){
        if(output.size()>=48&&output.compare(0,8,"ARSDRAW1")==0){
            _setmode(_fileno(stdout),_O_BINARY);std::cout.write(output.data(),output.size());return std::cout?0:15;
        }
        const std::vector<std::string> errors={"PACKAGE_BUDGET_REJECTED\n","PACKAGE_ENVELOPE_REJECTED\n","PACKAGE_ZIP_REJECTED\n","PACKAGE_MANIFEST_REJECTED\n","PACKAGE_PNG_REJECTED\n","PACKAGE_MOC_REJECTED\n","PACKAGE_PHYSICS_REJECTED\n","PACKAGE_OUTPUT_REJECTED\n"};
        if(std::find(errors.begin(),errors.end(),output)!=errors.end()){std::cout<<output;return 0;}
        std::cout<<"OUTPUT_REJECTED"<<std::endl;return 0;
    }
    const std::vector<std::string> allowed={"PREFLIGHT_OK\n","MODEL_REJECTED\n","INVALID_ENVELOPE\n","ISOLATION_DENIED\n",
                                          "ISOLATION_NO_CONNECTION_WINDOW\n","MEMORY_DENIED\n"};
    output.erase(std::remove(output.begin(),output.end(),'\r'),output.end());
    if(std::find(allowed.begin(),allowed.end(),output)==allowed.end()){std::cout<<"OUTPUT_REJECTED"<<std::endl;return 0;}
    std::cout<<output;return 0;
}
