#pragma once
#include "../../src/assets/asset_limits.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
#ifdef _WIN32
#include <malloc.h>
#endif
namespace test {
inline void check(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(1);
    }
}
struct Host {
    std::atomic<std::size_t> live{0}, calls{0}, fail_at{0};
    bool throwing = false, misalign = false;
    void *misaligned_original = nullptr;
    void (*on_first_allocate)(void *) = nullptr;
    void *callback_data = nullptr;
    static void *ARS_CALL allocate(void *u, std::uint64_t n, std::uint32_t a) {
        auto &h = *static_cast<Host *>(u);
        const auto call = ++h.calls;
        const auto requested_alignment = a;
        if (call == 1 && h.on_first_allocate)
            h.on_first_allocate(h.callback_data);
        if (call == 1 && h.throwing)
            throw 42;
        if (call == h.fail_at)
            return nullptr;
        if (a < alignof(void *))
            a = alignof(void *);
#ifdef _WIN32
        void *p = _aligned_malloc(n ? n : 1, a);
#else
        void *p = nullptr;
        if (posix_memalign(&p, a, n ? n : 1) != 0)
            p = nullptr;
#endif
        if (p)
            ++h.live;
        if (p && h.misalign && requested_alignment > 1 && !h.misaligned_original) {
            h.misaligned_original = p;
            return static_cast<char *>(p) + 1;
        }
        return p;
    }
    static void ARS_CALL release(void *u, void *p, std::uint64_t, std::uint32_t) {
        if (!p)
            return;
        auto &host = *static_cast<Host *>(u);
        --host.live;
        if (host.misaligned_original && p == static_cast<char *>(host.misaligned_original) + 1) {
            p = host.misaligned_original;
            host.misaligned_original = nullptr;
        }
#ifdef _WIN32
        _aligned_free(p);
#else
        std::free(p);
#endif
    }
};
inline std::vector<std::uint8_t> file(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary);
    check(bool(f), "fixture exists");
    return {std::istreambuf_iterator<char>(f), {}};
}
} // namespace test
