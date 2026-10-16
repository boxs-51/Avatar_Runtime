#include "../../src/validation/schema_worker_client.hpp"
#include "../../src/validation/strict_json.hpp"
#include "asset_test_support.hpp"
int main(int argc, char **argv) {
    test::check(argc == 3, "worker and reference corpus");
    ars::validation::SchemaClient client({std::filesystem::absolute(argv[1]).string()});
    test::Host host;
    {
        ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
        ars::assets::BoundedMemory memory(&resource, 64 * ars::assets::MiB);
        ars::validation::Document doc(&memory);
        ars::assets::Limits limits;
        for (auto input : {"{\"x\":1,\"x\":2}", "{\"x\":1,\"\\u0078\":2}", "{\"x\":1e400}", "[01]", "[true,]",
                           "[NaN]", "\"\\ud800\"", "{} trailing", "{\"x\":\"\xc0\xaf\"}"}) {
            ars::assets::Work work{limits.work};
            bool rejected = false;
            try {
                ars::validation::parse_json(input, doc, limits, work);
            } catch (const ars::assets::Failure &f) {
                rejected = f.status == ARS_E_BAD_ASSET;
            }
            test::check(rejected, "strict JSON hostile bytes rejected");
        }
        ars::assets::Work work{limits.work};
        ars::validation::parse_json("{\"number\":1,\"boolean\":true,\"unicode\":\"\\ud83d\\ude00\"}", doc,
                                    limits, work);
        auto m = doc.root();
        test::check(m.at("number").kind() != m.at("boolean").kind(), "bool distinct from numeric");
        test::check(m.at("unicode").string().size() == 4, "surrogate pair decoded");
        limits.nodes = 1;
        work.remaining = 100;
        bool budget = false;
        try {
            ars::validation::parse_json("[0]", doc, limits, work);
        } catch (const ars::assets::Failure &f) {
            budget = f.status == ARS_E_OUT_OF_BUDGET;
        }
        test::check(budget, "node preflight");
        limits = {};
        work.remaining = limits.work;
        const auto oracle = test::file(std::filesystem::path(argv[2]) / "schema-parity.json");
        ars::validation::parse_json({reinterpret_cast<const char *>(oracle.data()), oracle.size()}, doc,
                                    limits, work);
        for (std::size_t i = 0; i < doc.root().size(); ++i) {
            auto record = doc.root().at(i);
            const auto kind = record.at("schema").string() == "manifest" ? ars::validation::Schema::manifest
                                                                         : ars::validation::Schema::rig;
            const auto expected = record.at("valid").boolean() ? ARS_OK : ARS_E_BAD_ASSET;
            const auto actual = client.validate(kind, record.at("input").string(), resource);
            const auto id = record.at("id").string();
            std::printf("SCHEMA_PARITY %.*s reference=%u native=%u\n", static_cast<int>(id.size()), id.data(),
                        unsigned(expected), unsigned(actual));
            test::check(actual == expected, "actual Draft2020-12 reference parity");
        }
    }
    test::check(host.live == 0, "JSON tracked allocation cleanup");
    std::puts("PASS strict JSON syntax/UTF8/duplicate/finite/bool/node gates");
}
