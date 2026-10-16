#include "../../src/validation/strict_json.hpp"
#include "asset_test_support.hpp"
int main() {
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
    }
    test::check(host.live == 0, "JSON tracked allocation cleanup");
    std::puts("PASS strict JSON syntax/UTF8/duplicate/finite/bool/node gates");
}
