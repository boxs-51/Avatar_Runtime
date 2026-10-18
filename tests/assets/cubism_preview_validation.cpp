#include "../../src/assets/validated_cpu_model.hpp"
#include "asset_test_support.hpp"
#ifdef ARS_TEST_SCENE2D
#include "rendering/scene2d.hpp"
#endif

int main(int argc, char **argv) {
    test::check(argc == 3, "worker and converted bundle path");
    ars::validation::SchemaClient client({std::filesystem::absolute(argv[1]).string()});
    auto bytes = test::file(argv[2]);
    test::Host host;
    {
        ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
        std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
        const auto status = ars::assets::validate_bundle(bytes, client, resource, model);
        test::check(status == ARS_OK && bool(model), "converted bundle validates natively");
        test::check(model->parts().size() == 1 && model->layers().size() == 1 &&
                        model->parameters().empty() && !model->glb(),
                    "static preview has exactly one nonanimated part");
        test::check(model->canvas()[0] == model->image().width &&
                        model->canvas()[1] == model->image().height &&
                        model->parts()[0].rect[2] == model->image().width &&
                        model->parts()[0].rect[3] == model->image().height,
                    "preview geometry matches decoded image");
#ifdef ARS_TEST_SCENE2D
        ars::rendering::PreparedScene2D scene;
        test::check(ars::rendering::prepare_static_2d(ars::rendering::scene_input(*model), scene) == ARS_OK &&
                        scene.draws.size() == 1,
                    "converted sample prepares one 2D draw");
#endif
        std::printf("CUBISM_PREVIEW_VALID width=%u height=%u rgba=%zu\n", model->image().width,
                    model->image().height, model->image().rgba.size());
    }
    test::check(host.live == 0, "preview releases all host allocations");
}
