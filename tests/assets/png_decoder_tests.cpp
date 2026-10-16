#include "../../src/assets/validated_cpu_model.hpp"
#include "asset_test_support.hpp"
int main(int argc, char **argv) {
    test::check(argc == 3, "worker/corpus");
    ars::validation::SchemaClient client({std::filesystem::absolute(argv[1]).string()});
    for (auto name : {"png-truncated", "png-crc", "png-dimension"}) {
        test::Host host;
        auto bytes = test::file(std::filesystem::path(argv[2]) / (std::string(name) + ".arschar"));
        {
            ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
            std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
            const auto actual = ars::assets::validate_bundle(bytes, client, resource, model);
            const auto expected =
                std::string_view(name) == "png-dimension" ? ARS_E_OUT_OF_BUDGET : ARS_E_BAD_ASSET;
            std::printf("PNG %s expected=%u actual=%u\n", name, unsigned(expected), unsigned(actual));
            test::check(actual == expected && !model, "real PNG negative");
        }
        test::check(host.live == 0, "PNG error allocation cleanup");
    }
}
