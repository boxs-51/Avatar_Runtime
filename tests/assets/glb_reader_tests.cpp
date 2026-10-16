#include "../../src/assets/glb_reader.hpp"
#include "asset_test_support.hpp"
int main(int argc, char **argv) {
    test::check(argc == 2, "corpus path");
    test::Host host;
    const auto bytes = test::file(std::filesystem::path(argv[1]) / "positive-triangle.glb");
    {
        ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
        std::shared_ptr<const ars::assets::GlbModel> model;
        const auto status = ars::assets::ingest_glb(bytes, resource, model);
        std::printf("C2 native GLB expected=%u actual=%u published=%u\n", unsigned(ARS_OK),
                    unsigned(status), unsigned(bool(model)));
        test::check(status == ARS_OK && bool(model), "real triangle GLB CPU ingestion");
    }
    test::check(host.live == 0, "GLB allocation lifetime balanced");
}
