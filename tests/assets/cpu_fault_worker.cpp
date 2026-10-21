#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <array>
#include <fcntl.h>
#include <io.h>
int wmain() {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
#if ARS_CPU_FAULT_MODE==1
    Sleep(15000);return 42;
#elif ARS_CPU_FAULT_MODE==2
    RaiseException(EXCEPTION_ACCESS_VIOLATION,EXCEPTION_NONCONTINUABLE,0,nullptr);return 43;
#elif ARS_CPU_FAULT_MODE==3
    _setmode(_fileno(stdout),_O_BINARY);std::array<char,16384> bytes{};
    for(unsigned i=0;i<2048;++i)if(std::fwrite(bytes.data(),1,bytes.size(),stdout)!=bytes.size())break;return 0;
#else
    _setmode(_fileno(stdout),_O_BINARY);std::array<char,96> bytes{'A','R','S','C','P','U','F','N'};
    return std::fwrite(bytes.data(),1,bytes.size(),stdout)==bytes.size()?0:44;
#endif
}
