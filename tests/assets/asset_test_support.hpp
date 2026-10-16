#pragma once
#include "../../src/assets/asset_limits.hpp"
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <vector>
#ifdef _WIN32
#include <malloc.h>
#endif
namespace test {
inline void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
struct Host {
    std::atomic<std::size_t> live{0},calls{0},fail_at{0};
    static void* ARS_CALL allocate(void* u,std::uint64_t n,std::uint32_t a){
        auto& h=*static_cast<Host*>(u);if(++h.calls==h.fail_at)return nullptr;
        if(a<alignof(void*))a=alignof(void*);
#ifdef _WIN32
        void* p=_aligned_malloc(n?n:1,a);
#else
        void* p=nullptr;if(posix_memalign(&p,a,n?n:1)!=0)p=nullptr;
#endif
        if(p)++h.live;return p;
    }
    static void ARS_CALL release(void* u,void* p,std::uint64_t,std::uint32_t){if(!p)return;--static_cast<Host*>(u)->live;
#ifdef _WIN32
        _aligned_free(p);
#else
        std::free(p);
#endif
    }
};
inline std::vector<std::uint8_t> file(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);check(bool(f),"fixture exists");return {std::istreambuf_iterator<char>(f),{}};}
}
