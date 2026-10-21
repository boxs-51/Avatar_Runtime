#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <userenv.h>
#include <sddl.h>
#include "isolated_cpu_load.hpp"
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>
namespace ars::assets {
namespace {
std::atomic_flag asset_job=ATOMIC_FLAG_INIT;
std::atomic<void *> orphan{nullptr};
std::atomic_flag reaping=ATOMIC_FLAG_INIT;
#ifdef ARS_CPU_ISOLATION_TESTING
std::atomic<bool> hold_cleanup{false};
#endif
struct Handle {
    HANDLE value=nullptr;
    Handle()=default;
    Handle(const Handle &)=delete;
    ~Handle(){reset();}
    void reset(HANDLE next=nullptr) noexcept {if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=next;}
    bool valid()const noexcept{return value && value!=INVALID_HANDLE_VALUE;}
};
std::wstring quote(std::wstring_view s) {
    std::wstring out=L"\"";std::size_t slashes=0;
    for(auto c:s){if(c==L'\\'){++slashes;continue;}out.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;if(c==L'"')out+=L'\\';out+=c;}
    out.append(slashes*2,L'\\');return out+L"\"";
}
std::wstring random_name() {
    std::array<unsigned char,16> bytes{};
    require(BCryptGenRandom(nullptr,bytes.data(),static_cast<ULONG>(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0,ARS_E_INTERNAL);
    std::wstring name;for(auto b:bytes){name+=L"0123456789abcdef"[b>>4];name+=L"0123456789abcdef"[b&15];}return name;
}
std::filesystem::path module_folder() {
    HMODULE module=nullptr;
    require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&isolated_cpu_load),&module)!=FALSE,ARS_E_INTERNAL);
    std::wstring path(32768,L'\0');auto n=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
    require(n && n<path.size(),ARS_E_INTERNAL);path.resize(n);return std::filesystem::path(path).parent_path();
}
bool plain_path(const std::filesystem::path &path) {
    auto current=path.root_path();
    for(const auto &component:path.relative_path()) {
        current/=component;auto attributes=GetFileAttributesW(current.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
    }
    return true;
}
struct Security {
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),nullptr,FALSE};
    Security() {
        Handle token;require(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value)!=FALSE,ARS_E_INTERNAL);
        DWORD size=0;GetTokenInformation(token.value,TokenUser,nullptr,0,&size);std::vector<unsigned char> storage(size);
        require(size && GetTokenInformation(token.value,TokenUser,storage.data(),size,&size),ARS_E_INTERNAL);
        LPWSTR sid=nullptr;require(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(storage.data())->User.Sid,&sid)!=FALSE,ARS_E_INTERNAL);
        std::wstring acl=L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;";acl+=sid;acl+=L")";LocalFree(sid);
        require(ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&descriptor,nullptr)!=FALSE,ARS_E_INTERNAL);
        attributes.lpSecurityDescriptor=descriptor;
    }
    ~Security(){if(descriptor)LocalFree(descriptor);}
};
void pipe_pair(Handle &server,Handle &client,std::wstring name,bool input,Security &security) {
    name=L"\\\\.\\pipe\\Local\\ars-cpu-"+name;
    server.value=CreateNamedPipeW(name.c_str(),(input?PIPE_ACCESS_OUTBOUND:PIPE_ACCESS_INBOUND)|FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_NOWAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,0,&security.attributes);
    require(server.valid(),ARS_E_INTERNAL);
    SECURITY_ATTRIBUTES inherit=security.attributes;inherit.bInheritHandle=TRUE;
    client.value=CreateFileW(name.c_str(),input?GENERIC_READ:GENERIC_WRITE,0,&inherit,OPEN_EXISTING,0,nullptr);
    require(client.valid(),ARS_E_INTERNAL);
    const auto connected=ConnectNamedPipe(server.value,nullptr);require(connected || GetLastError()==ERROR_PIPE_CONNECTED,ARS_E_INTERNAL);
}
std::uint32_t fnv(std::span<const std::uint8_t> s){std::uint32_t hash=2166136261u;for(auto b:s){hash^=b;hash*=16777619u;}return hash;}
void put(std::span<std::uint8_t> s,std::size_t at,std::uint64_t v,unsigned n=4){for(unsigned i=0;i<n;++i)s[at+i]=static_cast<std::uint8_t>(v>>(8*i));}
} // namespace
#ifdef ARS_CPU_ISOLATION_TESTING
void hold_cpu_cleanup_for_test(bool hold) noexcept {hold_cleanup.store(hold);}
#endif
struct AdmissionCustody::State {
    Handle job,process;
    std::filesystem::path folder;
    std::wstring profile;
    bool gated=false,launched=false,clean=false;
};
AdmissionCustody::AdmissionCustody():state_(std::make_unique<State>()){}
AdmissionCustody::~AdmissionCustody(){
    if(state_->gated && !state_->clean) {
        // One global gated job means there can be at most one orphan. Transfer
        // preallocated custody without allocation or releasing the admission
        // gate. A later management/admission call must observe actual cleanup.
        void *expected=nullptr;auto *retained=state_.release();
        if(!orphan.compare_exchange_strong(expected,retained,std::memory_order_acq_rel))std::terminate();
        return;
    }
    if(state_->gated)asset_job.clear(std::memory_order_release);
}
bool AdmissionCustody::reap() noexcept { return reap_state(*state_); }
bool AdmissionCustody::reap_state(State &s) noexcept {
    if(s.clean)return true;
#ifdef ARS_CPU_ISOLATION_TESTING
    if(hold_cleanup.load())return false;
#endif
    if(s.process.valid() && WaitForSingleObject(s.process.value,0)!=WAIT_OBJECT_0) { TerminateProcess(s.process.value,1); return false; }
    if(s.job.valid()) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
        if(!QueryInformationJobObject(s.job.value,JobObjectBasicAccountingInformation,&info,sizeof(info),nullptr))return false;
        if(info.ActiveProcesses){TerminateJobObject(s.job.value,1);return false;}
    }
    if(s.launched) {
        const auto result=DeleteAppContainerProfile(s.profile.c_str());
        if(FAILED(result) && result!=HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && result!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND))return false;
    }
    if(!s.folder.empty()) {
        for(auto name:{L"worker.exe",L"worker-data.txt",L"launcher-state.json",L"job-cleanup.json",L"setup-error.json"}) {
            if(!DeleteFileW((s.folder/name).c_str()) && GetLastError()!=ERROR_FILE_NOT_FOUND)return false;
        }
        if(!RemoveDirectoryW(s.folder.c_str()) && GetLastError()!=ERROR_PATH_NOT_FOUND && GetLastError()!=ERROR_FILE_NOT_FOUND)return false;
    }
    s.process.reset();s.job.reset();s.clean=true;
    return true;
}
bool AdmissionCustody::reap_orphan() noexcept {
    if(reaping.test_and_set(std::memory_order_acq_rel))return false;
    auto *state=static_cast<State *>(orphan.exchange(nullptr,std::memory_order_acq_rel));
    bool complete=true;
    if(state) {
        complete=reap_state(*state);
        if(complete){delete state;asset_job.clear(std::memory_order_release);}
        else orphan.store(state,std::memory_order_release);
    }
    reaping.clear(std::memory_order_release);return complete;
}
ars_status isolated_cpu_load(std::span<const std::uint8_t> input,Resource &r,
    std::shared_ptr<const ValidatedCpuModel> &out,std::unique_ptr<AdmissionCustody> &pending) noexcept {
    if(pending || !AdmissionCustody::reap_orphan())return ARS_E_BUSY;
    if(input.empty() || input.size()>packet::byte_cap)return ARS_E_OUT_OF_BUDGET;
    ars_status status=ARS_E_INTERNAL;
    try {
        pending=std::make_unique<AdmissionCustody>();auto &s=*pending->state_;
        if(asset_job.test_and_set(std::memory_order_acq_rel)){pending.reset();return ARS_E_BUSY;}s.gated=true;
        packet::Identity identity;auto random=random_name();
        require(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&identity.nonce),sizeof(identity.nonce),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0 && identity.nonce,ARS_E_INTERNAL);
        require(packet::digest(input,identity.request)==ARS_OK,ARS_E_INTERNAL);
        const auto module=module_folder();const auto broker=module/L"ars_cpu_model_broker.exe",worker=module/L"ars_cpu_admission_worker.exe";
        require(plain_path(broker) && plain_path(worker),ARS_E_UNSUPPORTED);
        Handle broker_file,worker_file;
        broker_file.value=CreateFileW(broker.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        worker_file.value=CreateFileW(worker.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        require(broker_file.valid() && worker_file.valid(),ARS_E_UNSUPPORTED);
        Security security;std::wstring temp(32768,L'\0');auto size=GetTempPathW(static_cast<DWORD>(temp.size()),temp.data());require(size && size<temp.size(),ARS_E_INTERNAL);temp.resize(size);
        require(plain_path(temp),ARS_E_INTERNAL);
        const auto folder=std::filesystem::path(temp)/(L"ars-cpu-"+random);
        require(CreateDirectoryW(folder.c_str(),&security.attributes)!=FALSE,ARS_E_INTERNAL);s.folder=folder;
        require(plain_path(folder),ARS_E_INTERNAL);
        require(CopyFileW(worker.c_str(),(folder/L"worker.exe").c_str(),TRUE)!=FALSE,ARS_E_INTERNAL);
        const auto copied_attributes=GetFileAttributesW((folder/L"worker.exe").c_str());
        require(copied_attributes!=INVALID_FILE_ATTRIBUTES && SetFileAttributesW((folder/L"worker.exe").c_str(),((copied_attributes&~FILE_ATTRIBUTE_READONLY)?(copied_attributes&~FILE_ATTRIBUTE_READONLY):FILE_ATTRIBUTE_NORMAL)),ARS_E_INTERNAL);
        Handle sentinel;sentinel.value=CreateFileW((folder/L"worker-data.txt").c_str(),GENERIC_WRITE,0,&security.attributes,CREATE_NEW,0,nullptr);require(sentinel.valid(),ARS_E_INTERNAL);sentinel.reset();
        s.profile=L"AvatarMocImport-"+random;s.job.value=CreateJobObjectW(nullptr,nullptr);require(s.job.valid(),ARS_E_INTERNAL);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
        limits.BasicLimitInformation.ActiveProcessLimit=2;require(SetInformationJobObject(s.job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits))!=FALSE,ARS_E_INTERNAL);
        Handle input_pipe,input_child,output_pipe,output_child,error_child;
        pipe_pair(input_pipe,input_child,random+L"-in",true,security);pipe_pair(output_pipe,output_child,random+L"-out",false,security);
        SECURITY_ATTRIBUTES inherit=security.attributes;inherit.bInheritHandle=TRUE;
        error_child.value=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&inherit,OPEN_EXISTING,0,nullptr);require(error_child.valid(),ARS_E_INTERNAL);
        STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput=input_child.value;startup.StartupInfo.hStdOutput=output_child.value;startup.StartupInfo.hStdError=error_child.value;
        SIZE_T needed=0;InitializeProcThreadAttributeList(nullptr,1,0,&needed);std::vector<unsigned char> storage(needed);
        startup.lpAttributeList=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(storage.data());require(InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&needed)!=FALSE,ARS_E_INTERNAL);
        struct Attr { PPROC_THREAD_ATTRIBUTE_LIST ptr;~Attr(){DeleteProcThreadAttributeList(ptr);} } attributes{startup.lpAttributeList};
        HANDLE handles[]{input_child.value,output_child.value,error_child.value};
        require(UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),nullptr,nullptr)!=FALSE,ARS_E_INTERNAL);
        auto command=quote(broker.wstring())+L" "+quote((folder/L"worker.exe").wstring())+L" "+quote(s.profile)+L" --cpu-model-full";
        PROCESS_INFORMATION process{};
        require(CreateProcessW(broker.c_str(),command.data(),nullptr,nullptr,TRUE,EXTENDED_STARTUPINFO_PRESENT|CREATE_SUSPENDED|CREATE_NO_WINDOW,
            nullptr,folder.c_str(),&startup.StartupInfo,&process)!=FALSE,ARS_E_INTERNAL);
        s.process.value=process.hProcess;Handle thread;thread.value=process.hThread;
        if(!AssignProcessToJobObject(s.job.value,s.process.value)){TerminateProcess(s.process.value,1);WaitForSingleObject(s.process.value,500);fail(ARS_E_INTERNAL);}
        s.launched=true;require(ResumeThread(thread.value)!=DWORD(-1),ARS_E_INTERNAL);
        input_child.reset();output_child.reset();error_child.reset();thread.reset();
        std::array<std::uint8_t,32> header{};put(header,0,0x31565341);put(header,4,1);put(header,8,input.size());put(header,12,fnv(input));put(header,16,identity.nonce,8);
        std::pmr::vector<std::uint8_t> output(&r);std::size_t sent=0;bool ended=false;
        const auto deadline=GetTickCount64()+15000;
        for(;;) {
            if(GetTickCount64()>=deadline){TerminateJobObject(s.job.value,1);fail(ARS_E_OUT_OF_BUDGET);}
            if(input_pipe.valid()) {
                const auto chunk=sent<header.size()?std::span<const std::uint8_t>(header).subspan(sent):input.subspan(sent-header.size());
                if(chunk.empty())input_pipe.reset();
                else {DWORD written=0;const auto ok=WriteFile(input_pipe.value,chunk.data(),static_cast<DWORD>(std::min<std::size_t>(16384,chunk.size())),&written,nullptr);
                    if(!ok && GetLastError()!=ERROR_NO_DATA && GetLastError()!=ERROR_PIPE_BUSY){input_pipe.reset();}
                    else sent+=written;}
            }
            std::array<std::uint8_t,16384> buffer{};DWORD read=0;
            const auto ok=ReadFile(output_pipe.value,buffer.data(),static_cast<DWORD>(buffer.size()),&read,nullptr);
            if(read){require(read<=packet::byte_cap-output.size(),ARS_E_OUT_OF_BUDGET);output.insert(output.end(),buffer.begin(),buffer.begin()+read);}
            if(!ok && GetLastError()==ERROR_BROKEN_PIPE)ended=true;
            if(WaitForSingleObject(s.process.value,0)==WAIT_OBJECT_0 && (ended || !read))break;
            if(!read)Sleep(1);
        }
        DWORD exit=1;require(GetExitCodeProcess(s.process.value,&exit)!=FALSE && exit==0,ARS_E_INTERNAL);
        // Reap is an independent OS query; broker JSON is never admission proof.
        const auto cleanup_deadline=GetTickCount64()+500;
        while(!pending->reap() && GetTickCount64()<cleanup_deadline)Sleep(1);
        require(pending->reap(),ARS_E_BUSY);
        if(output.size()<packet::header_bytes) {
            const std::string_view text(reinterpret_cast<const char *>(output.data()),output.size());
            if(text.starts_with("TIMEOUT_REJECTED") || text.starts_with("CPU_REJECTED") || text.starts_with("OUTPUT_REJECTED"))fail(ARS_E_OUT_OF_BUDGET);
            fail(ARS_E_INTERNAL);
        }
        status=packet::decode(output,identity,r,out);
    } catch(const Failure &e){status=e.status;}catch(const std::bad_alloc &){status=ARS_E_OUT_OF_BUDGET;}catch(...){status=ARS_E_INTERNAL;}
    if(pending) {
        if(pending->state_->job.valid() && status!=ARS_OK)TerminateJobObject(pending->state_->job.value,1);
        if(pending->reap())pending.reset();
        else if(status==ARS_OK)status=ARS_E_BUSY;
    }
    return status;
}
} // namespace ars::assets
