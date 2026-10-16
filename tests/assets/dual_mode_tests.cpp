#include "../../src/assets/validated_cpu_model.hpp"
#include "asset_test_support.hpp"
int main(int argc, char **argv) {
    test::check(argc == 3, "worker and corpus");
    ars::validation::SchemaClient client({std::filesystem::absolute(argv[1]).string()});
    const std::filesystem::path root = argv[2];
    test::Host oracle_host;
    ars::assets::Resource oracle_resource(test::Host::allocate, test::Host::release, &oracle_host);
    ars::validation::Document document(&oracle_resource);
    auto oracle = test::file(root / "manifest.json");
    ars::assets::Limits limits;
    ars::assets::Work work{limits.work};
    ars::validation::parse_json({reinterpret_cast<const char *>(oracle.data()), oracle.size()}, document,
                                limits, work);
    unsigned cases = 0;
    for (std::size_t i = 0; i < document.root().size(); ++i) {
        auto row = document.root().at(i);
        const auto file = row.at("file").string();
        if (!file.ends_with(".arschar"))
            continue;
        const auto id = row.at("id").string(), verdict = row.at("expected").string();
        const auto expected = test::expected_status(verdict);
        auto bytes = test::file(root / std::string(file));
        test::Host host;
        {
            ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
            std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
            const auto actual = ars::assets::validate_bundle(bytes, client, resource, model);
            std::printf("C2_BUNDLE %.*s expected=%u actual=%u\n", static_cast<int>(id.size()), id.data(),
                        unsigned(expected), unsigned(actual));
            test::check(actual == expected && bool(model) == (actual == ARS_OK),
                        "C1 replay and C2 complete bundle oracle");
            if (model) {
                bytes.assign(bytes.size(), 0);
                const bool three = id == "positive-3d", dual = id.starts_with("positive-dual");
                test::check(bool(model->glb()) == (three || dual), "required representation never skipped");
                if (!three)
                    test::check(model->parts().size() == 2 && model->image().rgba[0] == 255,
                                "C2 retains actual 2D image and geometry");
                else
                    test::check(model->parts().empty() && model->image().rgba.empty(),
                                "3D-only model has no fabricated 2D data");
                if (three || dual)
                    test::check(model->glb()->data().accessors[0].values[3] == 1 &&
                                    model->glb()->metadata().at("asset").at("version").string() == "2.0",
                                "owned GLB and shared arena lifetime");
                const auto &catalog = model->semantics();
                for (std::size_t j = 1; j < catalog.size(); ++j)
                    test::check(catalog[j - 1].id < catalog[j].id, "canonical semantic catalog order");
                if (three || id == "positive-dual-full") {
                    bool bound = false;
                    for (const auto &s : catalog)
                        if (s.id == "mouth.open") {
                            test::check(s.target3d == "morph:MouthOpen" && s.has_default && s.has_minimum &&
                                            s.has_maximum && s.minimum == 0 && s.maximum == 1,
                                        "owned canonical bindings domains and defaults");
                            bound = true;
                        }
                    test::check(bound, "required canonical channel retained");
                }
            }
        }
        test::check(host.live == 0, "bundle rollback and model cleanup");
        ++cases;
    }
    for (auto cap : {7u, 8u}) {
        test::Host host;
        {
            ars::assets::Resource r(test::Host::allocate, test::Host::release, &host);
            ars::assets::Limits limits;
            limits.pixels = cap;
            std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
            const auto bytes = test::file(root / "positive-dual-full.arschar");
            const auto expected = cap == 8 ? ARS_OK : ARS_E_OUT_OF_BUDGET;
            test::check(ars::assets::validate_bundle(bytes, client, r, model, limits) == expected &&
                            bool(model) == (expected == ARS_OK),
                        "shared 2D plus 3D pixel boundary");
        }
        test::check(host.live == 0, "shared pixel rollback cleanup");
    }
    for (auto fixture : {"positive-3d.arschar", "positive-dual-full.arschar"}) {
        const auto bytes = test::file(root / fixture);
        std::size_t calls = 0;
        {
            test::Host host;
            {
                ars::assets::Resource r(test::Host::allocate, test::Host::release, &host);
                std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
                test::check(ars::assets::validate_bundle(bytes, client, r, model) == ARS_OK,
                            "dual fault baseline");
                calls = host.calls.load();
            }
            test::check(host.live == 0, "dual baseline balanced");
        }
        for (std::size_t cut = 1; cut <= calls; ++cut) {
            test::Host host;
            host.fail_at = cut;
            {
                ars::assets::Resource r(test::Host::allocate, test::Host::release, &host);
                std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
                test::check(ars::assets::validate_bundle(bytes, client, r, model) == ARS_E_OUT_OF_BUDGET &&
                                !model && r.live() == 0,
                            "every dual allocation cut atomic");
                host.fail_at = 0;
                test::check(ars::assets::validate_bundle(bytes, client, r, model) == ARS_OK,
                            "dual retry after allocation failure");
            }
            test::check(host.live == 0, "dual allocator balanced");
            std::printf("C2_BUNDLE_ALLOC %s cut=%zu/%zu PASS\n", fixture, cut, calls);
        }
    }
    std::printf("PASS %u C1/C2 bundle oracles and every 3D/dual allocation cut\n", cases);
}
