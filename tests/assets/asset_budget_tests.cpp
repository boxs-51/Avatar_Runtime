#include "../../src/assets/validated_cpu_model.hpp"
#include "asset_test_support.hpp"
int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    test::check(argc == 3, "worker/corpus");
    auto bytes = test::file(std::filesystem::path(argv[2]) / "positive-2d.arschar");
    ars::validation::SchemaClient client({std::filesystem::absolute(argv[1]).string()});
    std::size_t total_allocations = 0;
    for (auto fixture : {"positive-2d.arschar", "positive-deflate.arschar", "positive-png-palette.arschar"}) {
        bytes = test::file(std::filesystem::path(argv[2]) / fixture);
        std::size_t allocations = 0;
        {
            test::Host host;
            {
                ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
                std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
                test::check(ars::assets::validate_bundle(bytes, client, resource, model) == ARS_OK,
                            "baseline allocator trace");
                allocations = host.calls;
            }
            test::check(host.live == 0, "baseline cleanup");
        }
        for (std::size_t i = 1; i <= allocations; ++i) {
            std::printf("allocator %s cut %zu/%zu\n", fixture, i, allocations);
            test::Host host;
            host.fail_at = i;
            {
                ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
                std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
                const auto status = ars::assets::validate_bundle(bytes, client, resource, model);
                test::check(status == ARS_E_OUT_OF_BUDGET && !model, "every host allocation failure atomic");
                host.fail_at = 0;
                test::check(ars::assets::validate_bundle(bytes, client, resource, model) == ARS_OK,
                            "retry after allocation failure");
            }
            test::check(host.live == 0, "fault cleanup balanced");
        }
        total_allocations += allocations;
    }
    bytes = test::file(std::filesystem::path(argv[2]) / "positive-2d.arschar");
    for (auto mode : {0, 1}) {
        test::Host host;
        host.throwing = mode == 0;
        host.misalign = mode == 1;
        {
            ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
            std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
            test::check(ars::assets::validate_bundle(bytes, client, resource, model) == ARS_E_INTERNAL &&
                            !model,
                        "throwing/misaligned host fails cleanly");
        }
        test::check(host.live == 0, "invalid allocator cleanup");
    }
    {
        struct Reentry {
            const std::vector<std::uint8_t> *bytes;
            ars::validation::SchemaClient *client;
            ars::assets::Resource *resource;
            ars_status status = ARS_OK;
            static void call(void *p) {
                auto &c = *static_cast<Reentry *>(p);
                std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
                c.status = ars::assets::validate_bundle(*c.bytes, *c.client, *c.resource, model);
                test::check(!model, "reentry no partial model");
            }
        };
        test::Host host;
        ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
        Reentry reentry{&bytes, &client, &resource};
        host.on_first_allocate = Reentry::call;
        host.callback_data = &reentry;
        std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
        test::check(ars::assets::validate_bundle(bytes, client, resource, model) == ARS_OK &&
                        reentry.status == ARS_E_BUSY,
                    "allocator reentry BUSY without deadlock");
        model.reset();
        test::check(resource.live() == 0 && host.live == 0, "reentry balanced");
    }
    for (auto field : {0, 1, 2, 3}) {
        test::Host host;
        ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
        ars::assets::Limits limits;
        if (field == 0)
            limits.compressed = bytes.size() - 1;
        if (field == 1)
            limits.expanded = 1;
        if (field == 2)
            limits.entries = 1;
        if (field == 3)
            limits.work = 1;
        std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
        test::check(ars::assets::validate_bundle(bytes, client, resource, model, limits) ==
                            ARS_E_OUT_OF_BUDGET &&
                        !model,
                    "resource preflight caps");
    }
    std::printf("PASS every host allocation fail/retry cut: %zu; exact scope caps and callback containment\n",
                total_allocations);
}
