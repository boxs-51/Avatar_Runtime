#include "../src/assets/validated_cpu_model.hpp"
#include <cstdio>
#include <cstdlib>
namespace {
struct Host {
    std::size_t live = 0;
    static void *ARS_CALL allocate(void *u, std::uint64_t n, std::uint32_t a) {
        void *p = nullptr;
        if (a < alignof(void *))
            a = alignof(void *);
        if (posix_memalign(&p, a, n ? n : 1))
            return nullptr;
        ++static_cast<Host *>(u)->live;
        return p;
    }
    static void ARS_CALL release(void *u, void *p, std::uint64_t, std::uint32_t) {
        if (p) {
            --static_cast<Host *>(u)->live;
            std::free(p);
        }
    }
};
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    if (size > 1024 * 1024)
        return 0;
    static ars::validation::SchemaClient client({ARS_FUZZ_WORKER, 250});
    Host host;
    {
        ars::assets::Resource resource(Host::allocate, Host::release, &host, 256 * ars::assets::MiB);
        ars::assets::Limits limits;
        limits.compressed = 1024 * 1024;
        limits.expanded = 8 * ars::assets::MiB;
        limits.pixels = 8 * ars::assets::MiB;
        limits.work = 500000;
        std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
        const auto status = ars::assets::validate_bundle({data, size}, client, resource, model, limits);
        if ((status == ARS_OK) != bool(model))
            std::abort();
        model.reset();
        if (resource.live())
            std::abort();
    }
    if (host.live)
        std::abort();
    return 0;
}
