#include "../../src/assets/glb_reader.hpp"
#include "asset_test_support.hpp"
int main(int argc, char **argv) {
    test::check(argc == 2, "corpus path");
    test::Host initial_host;
    const std::filesystem::path root = argv[1];
    const auto initial_bytes = test::file(root / "positive-triangle.glb");
    {
        ars::assets::Resource resource(test::Host::allocate, test::Host::release, &initial_host);
        std::shared_ptr<const ars::assets::GlbModel> model;
        const auto status = ars::assets::ingest_glb(initial_bytes, resource, model);
        std::printf("C2 native GLB expected=%u actual=%u published=%u\n", unsigned(ARS_OK), unsigned(status),
                    unsigned(bool(model)));
        test::check(status == ARS_OK && bool(model), "real triangle GLB CPU ingestion");
        test::check(model->data().meshes.size() == 1 && model->data().meshes[0].primitives[0].indices[2] == 2,
                    "actual decoded triangle indices");
    }
    test::check(initial_host.live == 0, "GLB allocation lifetime balanced");
    test::Host oracle_host;
    ars::assets::Resource oracle_resource(test::Host::allocate, test::Host::release, &oracle_host);
    ars::validation::Document oracle(&oracle_resource);
    const auto file = test::file(root / "manifest.json");
    ars::assets::Limits limits;
    ars::assets::Work work{limits.work};
    ars::validation::parse_json({reinterpret_cast<const char *>(file.data()), file.size()}, oracle, limits,
                                work);
    unsigned cases = 0;
    for (std::size_t i = 0; i < oracle.root().size(); ++i) {
        auto row = oracle.root().at(i);
        const auto filename = row.at("file").string();
        if (row.at("stage").string() != "C2" || !filename.ends_with(".glb"))
            continue;
        const auto expected = row.at("expected").string();
        const auto status = test::expected_status(expected);
        test::Host host;
        auto bytes = test::file(root / std::string(filename));
        {
            ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
            std::shared_ptr<const ars::assets::GlbModel> model;
            const auto actual = ars::assets::ingest_glb(bytes, resource, model);
            std::printf("GLB %.*s expected=%u actual=%u\n", static_cast<int>(filename.size()),
                        filename.data(), unsigned(status), unsigned(actual));
            test::check(actual == status && bool(model) == (actual == ARS_OK),
                        "complete native GLB fixture oracle and atomic result");
            if (model) {
                bytes.assign(bytes.size(), 0);
                test::check(model->data().accessors[0].values[3] == 1 &&
                                model->metadata().at("asset").at("version").string() == "2.0",
                            "owned binary values and JSON lifetime");
                if (filename == "positive-glb-ignored-channel.glb")
                    test::check(model->data().animations[0].channels.size() == 2,
                                "node-less channel ignored without fabricated active target");
                if (filename == "positive-glb-full.glb" || filename == "positive-glb-cubic.glb") {
                    const auto &data = model->data();
                    test::check(data.skins.size() == 1 && data.animations.size() == 1 &&
                                    data.images.size() == 1,
                                "real skin animation and image metadata");
                    test::check(data.images[0].rgba.size() == 4 && data.images[0].rgba[0] == 255,
                                "real embedded PNG pixels");
                    test::check(data.meshes[0].target_names[0] == "MouthOpen" && data.nodes[1].name == "Head",
                                "owned named morph and node");
                    test::check(data.accessors[data.skins[0].inverse_bind].values[15] == 1,
                                "actual inverse bind values");
                }
            }
        }
        test::check(host.live == 0, "every GLB fixture cleanup");
        ++cases;
    }
    std::printf("PASS %u complete C2 GLB fixtures\n", cases);
}
