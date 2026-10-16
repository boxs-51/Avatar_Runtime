#include "../../src/assets/validated_cpu_model.hpp"
#include "../../src/validation/strict_json.hpp"
#include "asset_test_support.hpp"
int main(int argc, char **argv) {
    test::check(argc == 3, "worker/corpus");
    ars::validation::SchemaClient client({std::filesystem::absolute(argv[1]).string()});
    std::filesystem::path root = argv[2];
    const char *malformed[]{"duplicate-semantic", "bool-numeric",
                            "bad-default",        "missing-layer-binding",
                            "bad-mapping",        "dangling-mask",
                            "self-mask",          "triangle-oob",
                            "uv-length",          "atlas-oob",
                            "delta-length",       "duplicate-json",
                            "json-invalid-utf8",  "json-exponent-overflow"};
    auto run = [&](std::string name, ars_status expected) {
        test::Host host;
        auto bytes = test::file(root / (name + ".arschar"));
        {
            ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
            std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
            const auto actual = ars::assets::validate_bundle(bytes, client, resource, model);
            std::printf("BUNDLE %s expected=%u actual=%u\n", name.c_str(), unsigned(expected),
                        unsigned(actual));
            test::check(actual == expected, "compiled bundle oracle");
            test::check(bool(model) == (actual == ARS_OK), "atomic model result");
            if (model) {
                test::check(model->parts().size() == 2 && model->image().width == 1 &&
                                model->image().height == 1,
                            "actual immutable CPU data");
                test::check(model->image().rgba.size() == 4 && model->image().rgba[0] == 255,
                            "real decoded pixels");
                bytes.assign(bytes.size(), 0);
                test::check(model->parts()[0].vertices.size() == 3 && model->image().rgba[0] == 255,
                            "caller bytes lifetime owned");
            }
        }
        test::check(host.live == 0, "bundle cleanup balanced");
    };
    (void)malformed;
    test::Host oracle_host;
    ars::assets::Resource oracle_resource(test::Host::allocate, test::Host::release, &oracle_host);
    auto oracle_bytes = test::file(root / "manifest.json");
    ars::validation::Document document(&oracle_resource);
    ars::assets::Limits oracle_limits;
    ars::assets::Work oracle_work{oracle_limits.work};
    ars::validation::parse_json({reinterpret_cast<const char *>(oracle_bytes.data()), oracle_bytes.size()},
                                document, oracle_limits, oracle_work);
    const auto cases = document.root();
    unsigned executed = 0;
    for (std::size_t i = 0; i < cases.size(); ++i) {
        auto record = cases.at(i);
        if (record.at("stage").string() != "C1")
            continue;
        const auto expected = record.at("expected").string();
        const auto status = expected == "OK"              ? ARS_OK
                            : expected == "UNSUPPORTED"   ? ARS_E_UNSUPPORTED
                            : expected == "OUT_OF_BUDGET" ? ARS_E_OUT_OF_BUDGET
                                                          : ARS_E_BAD_ASSET;
        run(std::string(record.at("id").string()), status);
        ++executed;
    }
    run("positive-dual", ARS_E_UNSUPPORTED);
    std::printf("PASS all %u C1 fixture oracles plus C2-boundary rejection\n", executed);
}
