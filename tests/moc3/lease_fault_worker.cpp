#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <vector>
#include <fcntl.h>
#include <io.h>
int main(){
    HANDLE token=nullptr;DWORD size=0,container=0;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 2;
    bool isolated=GetTokenInformation(token,TokenIsAppContainer,&container,sizeof(container),&size)&&container;CloseHandle(token);if(!isolated)return 3;
    _setmode(_fileno(stdout),_O_BINARY);auto mode=std::cin.get();
    if(mode=='C'){LARGE_INTEGER q{};for(;;)QueryPerformanceCounter(&q);}
    if(mode=='W'){Sleep(15000);return 4;}
    if(mode=='M'){auto p=VirtualAlloc(nullptr,768ull*1024*1024,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(p){VirtualFree(p,0,MEM_RELEASE);return 5;}std::cout<<"DENIED";return 0;}
    if(mode=='O'){std::vector<char> data(1024*1024,'x');for(unsigned i=0;i<194;++i)std::cout.write(data.data(),data.size());return 0;}
    return 6;
}
