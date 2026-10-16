#include "../../src/assets/zip_reader.hpp"
#include "asset_test_support.hpp"
int main(int argc, char **argv) {
    test::check(argc == 2, "corpus path");
    const std::filesystem::path root = argv[1];
    for (const auto &entry : std::filesystem::directory_iterator(root)) {
        const auto name = entry.path().filename().string();
        if (!name.starts_with("path-") && !name.starts_with("alias-") && !name.starts_with("zip-") &&
            !(name.starts_with("positive-") && name.ends_with(".arschar")))
            continue;
        const auto bytes = test::file(entry.path());
        test::Host host;
        {
            ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
            ars::assets::OwnedBundle bundle(resource);
            ars::assets::Limits limits;
            ars::assets::Work work{limits.work};
            ars_status actual = ARS_OK;
            try {
                ars::assets::read_zip(bytes, bundle, resource, limits, work);
            } catch (const ars::assets::Failure &e) {
                actual = e.status;
            }
            const auto expected = name.starts_with("positive-")              ? ARS_OK
                                  : name.find("budget") != std::string::npos ? ARS_E_OUT_OF_BUDGET
                                                                             : ARS_E_BAD_ASSET;
            std::printf("ZIP %s expected=%u actual=%u\n", name.c_str(), unsigned(expected), unsigned(actual));
            test::check(actual == expected, "native archive oracle");
        }
        test::check(host.live == 0, "archive all allocations balanced");
    }
}
