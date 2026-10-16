#pragma once
#include "ars/ars.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <new>

namespace ars::assets {
inline constexpr std::size_t MiB = 1024 * 1024;
struct Failure {
    ars_status status;
};
[[noreturn]] inline void fail(ars_status s = ARS_E_BAD_ASSET) { throw Failure{s}; }
inline void require(bool ok, ars_status s = ARS_E_BAD_ASSET) {
    if (!ok)
        fail(s);
}
struct Limits {
    std::size_t compressed = 256 * MiB, expanded = 384 * MiB, entries = 4096, json = MiB;
    std::size_t depth = 64, nodes = 65536, path = 512, edge = 8192, pixels = 128 * MiB;
    std::size_t vertices = 2000000, work = 8000000, ratio = 1000;
    bool valid() const noexcept {
        const Limits ceiling;
        return compressed && compressed <= ceiling.compressed && expanded && expanded <= ceiling.expanded &&
               entries && entries <= ceiling.entries && json && json <= ceiling.json && depth &&
               depth <= ceiling.depth && nodes && nodes <= ceiling.nodes && path && path <= ceiling.path &&
               edge && edge <= ceiling.edge && pixels && pixels <= ceiling.pixels && vertices &&
               vertices <= ceiling.vertices && work && work <= ceiling.work && ratio &&
               ratio <= ceiling.ratio;
    }
};
// Caller-owned private CPU context. It must outlive every model allocated from it.
// OS helper reservations share this same cap; no runtime authority locks are held.
class Resource final : public std::pmr::memory_resource {
    ars_host_alloc_fn alloc_;
    ars_host_free_fn free_;
    void *user_;
    std::size_t cap_;
    std::atomic<std::size_t> live_{0}, peak_{0};
    std::atomic_flag job_ = ATOMIC_FLAG_INIT;
    void *do_allocate(std::size_t n, std::size_t a) override {
        require(a <= UINT32_MAX, ARS_E_OUT_OF_BUDGET);
        reserve(n);
        void *p = nullptr;
        try {
            p = alloc_(user_, n, static_cast<std::uint32_t>(a));
        } catch (...) {
            release(n);
            fail(ARS_E_INTERNAL);
        }
        if (!p) {
            release(n);
            throw std::bad_alloc();
        }
        if (reinterpret_cast<std::uintptr_t>(p) % a) {
            try {
                free_(user_, p, n, static_cast<std::uint32_t>(a));
            } catch (...) {
            }
            release(n);
            fail(ARS_E_INTERNAL);
        }
        return p;
    }
    void do_deallocate(void *p, std::size_t n, std::size_t a) override {
        try {
            free_(user_, p, n, static_cast<std::uint32_t>(a));
        } catch (...) {
        }
        release(n);
    }
    bool do_is_equal(const std::pmr::memory_resource &r) const noexcept override { return this == &r; }

  public:
    Resource(ars_host_alloc_fn a, ars_host_free_fn f, void *u, std::size_t cap = 1024 * MiB)
        : alloc_(a), free_(f), user_(u), cap_(cap) {
        require(a && f && cap && cap <= 1024 * MiB, ARS_E_INVALID_ARGUMENT);
    }
    void reserve(std::size_t n) {
        auto old = live_.load();
        do {
            if (n > cap_ - old)
                throw std::bad_alloc();
        } while (!live_.compare_exchange_weak(old, old + n));
        auto peak = peak_.load();
        while (peak < old + n && !peak_.compare_exchange_weak(peak, old + n)) {
        }
    }
    void release(std::size_t n) noexcept { live_.fetch_sub(n); }
    std::size_t live() const noexcept { return live_.load(); }
    std::size_t peak() const noexcept { return peak_.load(); }
    bool begin_job() noexcept { return !job_.test_and_set(); }
    void end_job() noexcept { job_.clear(); }
};
struct Reservation {
    Resource *resource;
    std::size_t bytes;
    Reservation(Resource &r, std::size_t n) : resource(&r), bytes(n) { r.reserve(n); }
    Reservation(const Reservation &) = delete;
    ~Reservation() { resource->release(bytes); }
};
struct Work {
    std::size_t remaining;
    void take(std::size_t n = 1) {
        require(n <= remaining, ARS_E_OUT_OF_BUDGET);
        remaining -= n;
    }
};
class BoundedMemory final : public std::pmr::memory_resource {
    std::pmr::memory_resource *upstream_;
    std::size_t live_ = 0, cap_;
    void *do_allocate(std::size_t n, std::size_t a) override {
        if (n > cap_ - live_)
            throw std::bad_alloc();
        auto *p = upstream_->allocate(n, a);
        live_ += n;
        return p;
    }
    void do_deallocate(void *p, std::size_t n, std::size_t a) override {
        upstream_->deallocate(p, n, a);
        live_ -= n;
    }
    bool do_is_equal(const std::pmr::memory_resource &r) const noexcept override { return this == &r; }

  public:
    BoundedMemory(std::pmr::memory_resource *up, std::size_t cap) : upstream_(up), cap_(cap) {}
};
} // namespace ars::assets
