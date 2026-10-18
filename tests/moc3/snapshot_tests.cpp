#include "independent_snapshot.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

using ars::moc3::Budget;
using ars::moc3::Parameter;
using ars::moc3::Result;
using ars::moc3::Session;
using ars::moc3::Snapshot;

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::cerr << "check failed at line " << __LINE__ << ": " #expression "\n"; \
        return 1; \
    } \
} while (false)

static std::uint64_t geometry_hash(const Snapshot& scene) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto& drawable : scene.drawables) {
        for (const auto& vertex : drawable.vertices) {
            const auto* bytes = reinterpret_cast<const unsigned char*>(&vertex);
            for (std::size_t i = 0; i < sizeof vertex; ++i) {
                hash = (hash ^ bytes[i]) * 1099511628211ull;
            }
        }
    }
    return hash;
}

int main(int argc, char** argv) {
    CHECK(argc == 2 || argc == 6);
    const auto test_root = std::filesystem::path(argv[1]);
    std::filesystem::create_directories(test_root);
    {
        std::ofstream out(test_root / "invalid.moc3", std::ios::binary);
        out << "NOPE" << std::string(64, '\0');
    }
    {
        std::ofstream out(test_root / "tiny.moc3", std::ios::binary);
        out << "MO";
    }
    {
        std::ofstream out(test_root.parent_path() / "moc3-escape.moc3", std::ios::binary);
        out << "NOPE" << std::string(64, '\0');
    }
    std::unique_ptr<Session> session;
    CHECK(Session::open(test_root, "../moc3-escape.moc3", Budget{}, session) == Result::invalid_path);
    CHECK(!session);

    const std::vector<std::uint8_t> invalid_blob{'N','O','P','E'};
    CHECK(Session::open_blob(invalid_blob, 1, Budget{}, session) == Result::invalid_model);
    CHECK(Session::open_blob(invalid_blob, 0, Budget{}, session) == Result::out_of_budget);
    CHECK(!session);
    CHECK(Session::open(test_root, "missing.moc3", Budget{}, session) == Result::invalid_path);
    CHECK(Session::open(test_root, "tiny.moc3", Budget{}, session) == Result::invalid_model);
    CHECK(Session::open(test_root, "invalid.moc3", Budget{}, session) == Result::invalid_model);
    CHECK(!session);

    if (argc == 6) {
        const auto root = std::filesystem::path(argv[2]);
        const auto relative = std::filesystem::path(argv[3]);
        const std::string parameter_id = argv[4];
        const float parameter_value = std::stof(argv[5]);
        std::ifstream source(root / relative, std::ios::binary);
        std::vector<std::uint8_t> blob{std::istreambuf_iterator<char>(source), {}};
        auto limited = Budget{};
        limited.max_moc_bytes = 8;
        CHECK(Session::open(root, relative, limited, session) == Result::out_of_budget);
        CHECK(!session);
        limited = Budget{};
        limited.max_model_bytes = 1;
        CHECK(Session::open(root, relative, limited, session) == Result::out_of_budget);
        CHECK(!session);
        limited = Budget{};
        limited.max_snapshot_bytes = sizeof(Snapshot);
        CHECK(Session::open(root, relative, limited, session) == Result::out_of_budget);
        CHECK(!session);
        limited = Budget{};
        limited.max_atlases = 0;
        CHECK(Session::open(root, relative, limited, session) == Result::out_of_budget);
        CHECK(!session);
        CHECK(Session::open(root, relative, Budget{}, session) == Result::ok);
        const auto baseline = session->current();
        CHECK(baseline && !baseline->drawables.empty());
        const auto baseline_hash = geometry_hash(*baseline);
        const auto original_drawables = baseline->drawables;
        // Every declared open/snapshot checkpoint fails transactionally. The
        // existing out session and its immutable snapshot must survive intact.
        for (int fault = 0; fault < 5 + static_cast<int>(baseline->drawables.size()); ++fault) {
            auto* prior = session.get();
            CHECK(Session::open_blob(blob, Budget{}.max_atlases, Budget{}, session, fault)
                  == Result::allocation_failed);
            CHECK(session.get() == prior && session->current() == baseline);
            CHECK(session->current()->drawables == original_drawables);
        }
        std::unique_ptr<Session> copied;
        CHECK(Session::open_blob(blob, Budget{}.max_atlases, Budget{}, copied) == Result::ok);
        std::fill(blob.begin(), blob.end(), 0); // caller buffer can be changed/released
        CHECK(copied->update_complete({}) == Result::ok);
        CHECK(copied->current()->drawables == original_drawables);
        CHECK(Session::open_blob(blob, Budget{}.max_atlases, Budget{}, copied) == Result::invalid_model);
        CHECK(copied->current()->drawables == original_drawables);
        CHECK(session->update({{"missing-parameter", 1}}) == Result::invalid_parameter);
        CHECK(session->current() == baseline);
        CHECK(session->update({{parameter_id, std::numeric_limits<float>::infinity()}})
              == Result::invalid_parameter);
        CHECK(session->current() == baseline);
        CHECK(session->update({{parameter_id, parameter_value},
                               {parameter_id, parameter_value}}) == Result::invalid_parameter);
        CHECK(session->current() == baseline);

        std::vector<Parameter> pose{{parameter_id, parameter_value}};
        session->fail_snapshot_checkpoint_after(0);
        CHECK(session->update(pose) == Result::allocation_failed);
        CHECK(session->current() == baseline);
        session->fail_snapshot_checkpoint_after(1);
        CHECK(session->update(pose) == Result::allocation_failed);
        CHECK(session->current() == baseline);
        session->fail_snapshot_checkpoint_after(-1);
        CHECK(session->update(pose) == Result::ok);
        const auto changed = session->current();
        CHECK(changed && changed->generation == baseline->generation + 1);
        const auto changed_hash = geometry_hash(*changed);
        CHECK(changed_hash != baseline_hash);
        CHECK(session->update(pose) == Result::ok);
        CHECK(geometry_hash(*session->current()) == changed_hash);
        CHECK(geometry_hash(*baseline) == baseline_hash); // old immutable scene remains live

        CHECK(session->update_complete(pose) == Result::ok);
        CHECK(geometry_hash(*session->current()) == changed_hash);
        const auto complete_changed = session->current();
        CHECK(session->update_complete({{parameter_id, std::numeric_limits<float>::infinity()}})
              == Result::invalid_parameter);
        CHECK(session->current() == complete_changed);
        CHECK(session->update_complete({{parameter_id, parameter_value},
                                        {parameter_id, parameter_value}})
              == Result::invalid_parameter);
        CHECK(session->current() == complete_changed);
        CHECK(session->update_complete({}) == Result::ok);
        CHECK(session->current()->drawables == original_drawables);
        CHECK(session->update_complete({}) == Result::ok);
        CHECK(session->current()->drawables == original_drawables);

        Result wrong_thread = Result::ok;
        std::thread worker([&] { wrong_thread = session->update_complete({}); });
        worker.join();
        CHECK(wrong_thread == Result::wrong_thread);
        CHECK(geometry_hash(*session->current()) == baseline_hash);
        session.reset();
        CHECK(geometry_hash(*baseline) == baseline_hash);
    }
    std::cout << "MOC3_SNAPSHOT_PASS\n";
}
