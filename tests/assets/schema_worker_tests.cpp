#include "../../src/validation/schema_worker_client.hpp"
#include "asset_test_support.hpp"
#include <barrier>
#include <chrono>
#include <cstring>
#include <thread>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/resource.h>
#endif
#ifdef ARS_SCHEMA_FAKE
int main() {
    using namespace ars::validation;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#else
    rlimit limit{worker_memory, worker_memory};
    if (setrlimit(RLIMIT_AS, &limit))
        return 2;
#endif
    Message request{};
    if (std::fread(request.data(), 1, 32, stdin) != 32)
        return 2;
    const auto n = get(request, 12);
    if (n > max_request)
        return 2;
    std::string body(n, '\0');
    if (std::fread(body.data(), 1, n, stdin) != n)
        return 2;
    if (body == "fake_sleep") {
        std::this_thread::sleep_for(std::chrono::seconds(20));
        return 0;
    }
    if (body == "fake_crash")
        return 17;
    Message reply = request;
    put(reply, 8, ARS_OK);
    put(reply, 24, 0);
    if (body == "fake_oom") {
        auto *p = std::malloc(worker_memory * 2);
        const bool refused = p == nullptr;
        if (p) {
            std::memset(p, 1, worker_memory * 2);
            std::free(p);
        }
        put(reply, 8, refused ? ARS_E_OUT_OF_BUDGET : ARS_E_INTERNAL);
    }
    if (body == "fake_nonce")
        put(reply, 16, get(reply, 16) ^ 1u);
    if (body == "fake_version")
        put(reply, 4, schema_protocol + 1);
    if (body == "fake_status")
        put(reply, 8, 999);
    if (body == "fake_short")
        return std::fwrite(reply.data(), 1, 7, stdout) == 7 ? 0 : 2;
    if (std::fwrite(reply.data(), 1, 32, stdout) != 32)
        return 2;
    if (body == "fake_long")
        std::fputc(0, stdout);
    return 0;
}
#else
int main(int argc, char **argv) {
    test::check(argc == 4, "worker/fake/resource-proof executable arguments");
    test::Host host;
    ars::assets::Resource resource(test::Host::allocate, test::Host::release, &host);
    ars::validation::SchemaClient client({std::filesystem::absolute(argv[1]).string()});
    // Frozen rig schema, semantically trivial but structurally valid.
    const auto text =
        R"({"schema_version":"0.1.0","coordinate_space":"canvas_px","canvas":{"width":1,"height":1},"parameters":[],"parts":[{"id":"p","z_index":0,"atlas_rect_px":[0,0,1,1],"vertices_px":[[0,0],[1,0],[0,1]],"uv":[[0,0],[1,0],[0,1]],"triangles":[[0,1,2]]}]})";
    ars::validation::WorkerResult evidence;
    auto status = client.validate(ars::validation::Schema::rig, text, resource, &evidence);
    std::printf("native schema actual=%u expected=%u\n", unsigned(status), unsigned(ARS_OK));
    test::check(status == ARS_OK, "bounded production schema helper validation");
    test::check(evidence.reaped && evidence.process_id != 0, "successful child reaped");
    test::check(client.validate(ars::validation::Schema::rig, "{}", resource) == ARS_E_BAD_ASSET,
                "actual schema rejection");
    test::check(client.validate(ars::validation::Schema::rig, "{", resource) == ARS_E_BAD_ASSET,
                "worker parse rejection");
    std::string huge(ars::validation::max_request + 1, 'x');
    test::check(client.validate(ars::validation::Schema::rig, huge, resource) == ARS_E_OUT_OF_BUDGET,
                "oversized input before spawn");
    ars::assets::Resource tiny(test::Host::allocate, test::Host::release, &host, 1024);
    test::check(client.validate(ars::validation::Schema::rig, text, tiny, &evidence) == ARS_E_OUT_OF_BUDGET &&
                    !evidence.process_id,
                "worker reservation before launch");
    ars::validation::SchemaClient relative({"worker"});
    test::check(relative.validate(ars::validation::Schema::rig, text, resource) == ARS_E_INVALID_ARGUMENT,
                "no PATH search");
    const auto fake = std::filesystem::absolute(argv[2]).string();
    ars::validation::SchemaClient faults({fake, 1000});
    for (auto input :
         {"fake_crash", "fake_nonce", "fake_version", "fake_status", "fake_short", "fake_long"}) {
        test::check(faults.validate(ars::validation::Schema::rig, input, resource, &evidence) ==
                        ARS_E_INTERNAL,
                    "invalid/crashed helper denied");
        test::check(evidence.reaped, "failed child reaped");
        std::printf("PASS %s cleanup pid=%llu\n", input,
                    static_cast<unsigned long long>(evidence.process_id));
    }
    test::check(faults.validate(ars::validation::Schema::rig, "fake_oom", resource, &evidence) ==
                        ARS_E_OUT_OF_BUDGET &&
                    evidence.reaped,
                "actual OS worker memory ceiling");
    ars::validation::SchemaClient memory_proof({std::filesystem::absolute(argv[3]).string()});
    test::check(memory_proof.validate(ars::validation::Schema::rig, "\"resource_probe\"", resource,
                                      &evidence) == ARS_E_OUT_OF_BUDGET &&
                    evidence.reaped,
                "production worker OS setup rejects 256 MiB allocation");
    ars::validation::SchemaClient timeout({fake, 50});
    const auto before = std::chrono::steady_clock::now();
    test::check(timeout.validate(ars::validation::Schema::rig, "fake_sleep", resource, &evidence) ==
                        ARS_E_OUT_OF_BUDGET &&
                    evidence.reaped,
                "deadline kills/reaps child");
    test::check(std::chrono::steady_clock::now() - before < std::chrono::seconds(5),
                "timeout cleanup bounded");
    std::barrier start(8);
    std::array<std::thread, 8> threads;
    std::atomic<unsigned> accepted{0};
    for (auto &thread : threads)
        thread = std::thread([&] {
            start.arrive_and_wait();
            const auto s = client.validate(ars::validation::Schema::rig, text, resource);
            test::check(s == ARS_OK || s == ARS_E_BUSY, "concurrent client typed result");
            if (s == ARS_OK)
                ++accepted;
        });
    for (auto &thread : threads)
        thread.join();
    test::check(accepted > 0 && resource.peak() <= 130 * ars::assets::MiB,
                "one worker reservation per client");
    test::check(resource.live() == 0 && host.live == 0, "schema lifetime balanced");
    std::printf("PASS worker caps/protocol/failure/deadline/concurrency; accepted=%u\n", accepted.load());
}
#endif
