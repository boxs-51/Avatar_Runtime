#include "../../src/assets/glb_reader.hpp"
#include "asset_test_support.hpp"
int main(int argc, char **argv) {
    test::check(argc == 2, "GLB corpus");
    const std::filesystem::path root = argv[1];
    std::size_t cuts = 0;
    for (auto fixture : {"positive-triangle.glb", "positive-glb-full.glb", "positive-glb-sparse.glb",
                         "positive-glb-cubic.glb"}) {
        const auto bytes = test::file(root / fixture);
        std::size_t calls = 0;
        {
            test::Host host;
            {
                ars::assets::Resource r(test::Host::allocate, test::Host::release, &host);
                std::shared_ptr<const ars::assets::GlbModel> out;
                test::check(ars::assets::ingest_glb(bytes, r, out) == ARS_OK, "GLB fault baseline");
                calls = host.calls.load();
            }
            test::check(host.live == 0, "GLB fault baseline balanced");
        }
        for (std::size_t i = 1; i <= calls; ++i) {
            test::Host host;
            host.fail_at = i;
            {
                ars::assets::Resource r(test::Host::allocate, test::Host::release, &host);
                std::shared_ptr<const ars::assets::GlbModel> out;
                const auto status = ars::assets::ingest_glb(bytes, r, out);
                test::check(status == ARS_E_OUT_OF_BUDGET && !out && r.live() == 0,
                            "every GLB allocator failure atomic");
                host.fail_at = 0;
                test::check(ars::assets::ingest_glb(bytes, r, out) == ARS_OK,
                            "GLB retry after allocator fault");
            }
            test::check(host.live == 0, "GLB fault cut cleanup");
            std::printf("GLB_ALLOC %s cut=%zu/%zu PASS\n", fixture, i, calls);
            ++cuts;
        }
    }
    const auto bytes = test::file(root / "positive-glb-full.glb");
    for (unsigned field = 0; field < 7; ++field) {
        test::Host host;
        ars::assets::Resource r(test::Host::allocate, test::Host::release, &host);
        ars::assets::Limits limits;
        if (field == 0)
            limits.expanded = bytes.size() - 1;
        if (field == 1)
            limits.json = 8;
        if (field == 2)
            limits.nodes = 1;
        if (field == 3)
            limits.depth = 1;
        if (field == 4)
            limits.work = 1;
        if (field == 5)
            limits.vertices = 2;
        if (field == 6)
            limits.pixels = 3;
        std::shared_ptr<const ars::assets::GlbModel> model;
        const auto status = ars::assets::ingest_glb(bytes, r, model, limits);
        std::printf("GLB_BUDGET field=%u expected=%u actual=%u\n", field, unsigned(ARS_E_OUT_OF_BUDGET),
                    unsigned(status));
        test::check(status == ARS_E_OUT_OF_BUDGET && !model && r.live() == 0 && host.live == 0,
                    "GLB preflight budgets and rollback");
    }
    for (auto cap : {7u, 8u}) {
        test::Host image_host;
        {
            ars::assets::Resource r(test::Host::allocate, test::Host::release, &image_host);
            ars::assets::Limits limits;
            limits.pixels = cap;
            std::shared_ptr<const ars::assets::GlbModel> model;
            const auto two_images = test::file(root / "glb-image-second.glb");
            const auto expected = cap == 8 ? ARS_OK : ARS_E_OUT_OF_BUDGET;
            test::check(ars::assets::ingest_glb(two_images, r, model, limits) == expected &&
                            bool(model) == (expected == ARS_OK),
                        "aggregate GLB image boundary");
        }
        test::check(image_host.live == 0, "aggregate GLB image cleanup");
    }
    for (bool throwing : {false, true}) {
        test::Host host;
        host.throwing = throwing;
        host.misalign = !throwing;
        ars::assets::Resource r(test::Host::allocate, test::Host::release, &host);
        std::shared_ptr<const ars::assets::GlbModel> model;
        test::check(ars::assets::ingest_glb(bytes, r, model) == ARS_E_INTERNAL && !model && r.live() == 0 &&
                        host.live == 0,
                    "GLB host callback containment");
    }
    test::Host host;
    {
        ars::assets::Resource r(test::Host::allocate, test::Host::release, &host);
        std::shared_ptr<const ars::assets::GlbModel> model;
        test::check(ars::assets::ingest_glb(bytes, r, model) == ARS_OK, "GLB old model fixture");
        auto old = model;
        const auto bad = test::file(root / "glb-index-oob.glb");
        test::check(ars::assets::ingest_glb(bad, r, model) == ARS_E_BAD_ASSET && !model,
                    "failed replacement has no partial result");
        test::check(old->data().meshes[0].primitives[0].indices[2] == 2,
                    "prior separately held GLB remains intact");
        test::check(r.begin_job(), "test owns resource admission");
        test::check(ars::assets::ingest_glb(bytes, r, model) == ARS_E_BUSY && !model, "GLB busy no reentry");
        r.end_job();
    }
    test::check(host.live == 0, "GLB final ownership balanced");
    std::printf("PASS %zu GLB allocator failure/retry cuts, budgets and immutable lifetime\n", cuts);
}
