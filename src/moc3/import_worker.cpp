#include <winsock2.h>
#include <windows.h>
#include "independent_snapshot.hpp"
#ifdef ARS_MOC3_PACKAGE_ADMISSION
#include "package_admission.hpp"
#endif
#include <cstdint>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <vector>

int wmain(int argc,wchar_t** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    HANDLE token=nullptr;DWORD actual=0,is_container=0;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 20;
    const bool isolated=GetTokenInformation(token,TokenIsAppContainer,&is_container,sizeof(is_container),&actual)&&is_container;
    DWORD cap_size=0;GetTokenInformation(token,TokenCapabilities,nullptr,0,&cap_size);
    std::vector<unsigned char> cap_data(cap_size);
    const bool zero_caps=cap_size>=sizeof(DWORD)&&GetTokenInformation(token,TokenCapabilities,cap_data.data(),cap_size,&cap_size)&&
        reinterpret_cast<TOKEN_GROUPS*>(cap_data.data())->GroupCount==0;
    CloseHandle(token);if(!isolated||!zero_caps)return 21;
    if(argc<2)return 22;
    const std::wstring mode=argv[1];
    if(mode==L"--hang"){Sleep(15000);return 23;}
    if(mode==L"--cpu"){LARGE_INTEGER value{};for(;;)QueryPerformanceCounter(&value);}
    if(mode==L"--crash"){RaiseException(EXCEPTION_ACCESS_VIOLATION,EXCEPTION_NONCONTINUABLE,0,nullptr);return 24;}
    if(mode==L"--flood"){for(int i=0;i<100000;++i)std::cout<<"untrusted";return 25;}
    if(mode==L"--memory"){
        void* memory=VirtualAlloc(nullptr,768ull*1024*1024,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if(memory){VirtualFree(memory,0,MEM_RELEASE);return 26;}
        std::cout<<"MEMORY_DENIED"<<std::endl;return 0;
    }
    if(mode==L"--probe"&&argc==4){
        wchar_t environment_canary[64]{};
        const auto environment_size=GetEnvironmentVariableW(L"ARS_MOC3_IMPORT_PROBE_ENV_CANARY",environment_canary,64);
        const bool environment_clean=environment_size==0&&GetLastError()==ERROR_ENVVAR_NOT_FOUND;
        auto file=CreateFileW(argv[2],GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        const bool read_denied=file==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_ACCESS_DENIED;
        if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
        auto write_name=std::wstring(argv[2])+L".write";
        file=CreateFileW(write_name.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        const bool write_denied=file==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_ACCESS_DENIED;
        if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
        WSADATA data{};bool network_denied=false,network_window=false;
        int network_code=0;
        if(WSAStartup(MAKEWORD(2,2),&data)==0){
            const auto socket_handle=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
            if(socket_handle==INVALID_SOCKET){network_code=WSAGetLastError();network_denied=network_code==WSAEACCES;}
            else{
                u_long nonblocking=1;ioctlsocket(socket_handle,FIONBIO,&nonblocking);
                sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
                address.sin_port=htons(static_cast<unsigned short>(std::stoul(argv[3])));
                const auto connected=connect(socket_handle,reinterpret_cast<sockaddr*>(&address),sizeof(address));
                const auto error=connected==SOCKET_ERROR?WSAGetLastError():0;
                network_code=error;
                network_denied=error==WSAEACCES;
                if(error==WSAEWOULDBLOCK){
                    WSAPOLLFD pending{socket_handle,POLLWRNORM,0};
                    const auto polled=WSAPoll(&pending,1,2000);network_window=polled==0;
                    network_code=polled==0?700:polled<0?WSAGetLastError():701;
                    if(polled>0){
                        int result=0,size=sizeof(result);
                        if(getsockopt(socket_handle,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&result),&size)==0)
                            {network_code=result;network_denied=result==WSAEACCES;}
                    }
                }
                closesocket(socket_handle);
            }
            WSACleanup();
        }
        wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
        auto own=std::wstring(executable);own.resize(own.find_last_of(L"\\/")+1);own+=L"worker-data.txt";
        file=CreateFileW(own.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        const bool own_read=file!=INVALID_HANDLE_VALUE;if(own_read)CloseHandle(file);
        file=CreateFileW(own.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
        const bool own_write_denied=file==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_ACCESS_DENIED;
        if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
        std::wstring command=L"\""+std::wstring(executable)+L"\" --hang";
        const bool spawned=CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)!=FALSE;
        if(spawned){TerminateProcess(child.hProcess,1);WaitForSingleObject(child.hProcess,5000);CloseHandle(child.hThread);CloseHandle(child.hProcess);}
        if(read_denied&&write_denied&&(network_denied||network_window)&&!spawned&&own_read&&own_write_denied&&environment_clean){
            std::cout<<(network_denied?"ISOLATION_DENIED":"ISOLATION_NO_CONNECTION_WINDOW")<<std::endl;return 0;
        }
        if(!read_denied)return 30;if(!write_denied)return 31;if(!network_denied)return 100000+network_code;
        if(spawned)return 33;if(!own_read)return 34;if(!own_write_denied)return 35;
        if(!environment_clean)return 36;
        return 27;
    }
#ifdef ARS_MOC3_PACKAGE_ADMISSION
    if(mode==L"--package"||mode==L"--evaluate"||mode==L"--simulate"||mode==L"--step"||mode==L"--lease"){
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        std::ios::sync_with_stdio(false);return ars::moc3::admission::package_worker(mode!=L"--package"&&mode!=L"--lease",mode==L"--simulate"||mode==L"--step"||mode==L"--lease",mode==L"--step",mode==L"--lease");
    }
#endif
    if(mode!=L"--moc")return 28;
    _setmode(_fileno(stdin),_O_BINARY);
    std::uint32_t length=0,textures=0;
    if(!std::cin.read(reinterpret_cast<char*>(&length),sizeof(length))||
       !std::cin.read(reinterpret_cast<char*>(&textures),sizeof(textures))||
       length<4||length>16*1024*1024||textures==0||textures>16){std::cout<<"INVALID_ENVELOPE"<<std::endl;return 0;}
    try{
        std::vector<std::uint8_t> bytes(length);
        if(!std::cin.read(reinterpret_cast<char*>(bytes.data()),length)||std::cin.peek()!=EOF){std::cout<<"INVALID_ENVELOPE"<<std::endl;return 0;}
        std::unique_ptr<ars::moc3::Session> session;
        const auto result=ars::moc3::Session::open_blob(bytes,textures,ars::moc3::Budget{},session);
        std::cout<<(result==ars::moc3::Result::ok?"PREFLIGHT_OK":"MODEL_REJECTED")<<std::endl;
        return 0;
    }catch(const std::bad_alloc&){std::cout<<"MODEL_REJECTED"<<std::endl;return 0;}
}
