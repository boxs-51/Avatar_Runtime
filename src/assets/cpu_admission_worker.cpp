#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "validated_cpu_model.hpp"
#include "../validation/schema_worker_protocol.hpp"
#include "schema_literals.hpp"
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <malloc.h>
#include <vector>

namespace {
using namespace ars;
constexpr std::uint32_t magic = 0x31565341; // ASV1, diagnostic verdict only
constexpr std::size_t input_cap = 16 * assets::MiB;
bool isolated() noexcept {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    DWORD size = 0, app = 0;
    BOOL job = FALSE;
    const bool container = GetTokenInformation(token, TokenIsAppContainer, &app, sizeof(app), &size) && app;
    GetTokenInformation(token, TokenCapabilities, nullptr, 0, &size);
    // No variable-size allocation before containment is established.
    alignas(TOKEN_GROUPS) unsigned char groups[sizeof(TOKEN_GROUPS)]{};
    const bool caps = size <= sizeof(groups) &&
        GetTokenInformation(token, TokenCapabilities, groups, sizeof(groups), &size) &&
        reinterpret_cast<const TOKEN_GROUPS *>(groups)->GroupCount == 0;
    CloseHandle(token);
    return container && caps && IsProcessInJob(GetCurrentProcess(), nullptr, &job) && job;
}
ars_status schema_check(void *, validation::Schema kind, std::string_view text, assets::Resource &) noexcept {
    if (!isolated())
        return ARS_E_INTERNAL;
    if ((kind != validation::Schema::manifest && kind != validation::Schema::rig) ||
        text.empty() || text.size() > validation::max_request)
        return ARS_E_BAD_ASSET;
    try {
        const auto options = jsoncons::json_options{}.max_nesting_depth(64);
        const auto instance = jsoncons::json::parse(text, options);
        // Frozen local schemas only. No external resolver or asset-supplied schema.
        const auto schema = jsoncons::jsonschema::make_json_schema(jsoncons::json::parse(
            kind == validation::Schema::manifest ? manifest_schema : rig_schema));
        return schema.is_valid(instance) ? ARS_OK : ARS_E_BAD_ASSET;
    } catch (const std::bad_alloc &) {
        return ARS_E_OUT_OF_BUDGET;
    } catch (const jsoncons::ser_error &) {
        return ARS_E_BAD_ASSET;
    } catch (...) {
        return ARS_E_INTERNAL;
    }
}
void *allocate(void *, std::uint64_t bytes, std::uint32_t alignment) {
    return _aligned_malloc(static_cast<std::size_t>(bytes), alignment);
}
void release(void *, void *p, std::uint64_t, std::uint32_t) { _aligned_free(p); }
} // namespace

int wmain(int argc, wchar_t **argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    if (!isolated())
        return 21;
#ifdef ARS_CPU_ADMISSION_TEST_FAULTS
    if (argc == 2) {
        if (std::wcscmp(argv[1], L"--fault-hang") == 0) { Sleep(15000); return 30; }
        if (std::wcscmp(argv[1], L"--fault-cpu") == 0) {
            LARGE_INTEGER now{};
            for (;;) QueryPerformanceCounter(&now);
        }
        if (std::wcscmp(argv[1], L"--fault-crash") == 0) {
            RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0, nullptr); return 31;
        }
        if (std::wcscmp(argv[1], L"--fault-memory") == 0) {
            void *memory = VirtualAlloc(nullptr, 768ull * 1024 * 1024, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (memory) { VirtualFree(memory, 0, MEM_RELEASE); return 32; }
            _setmode(_fileno(stdout), _O_BINARY);
            validation::Message proof{};
            validation::put(proof, 0, magic); validation::put(proof, 4, 1);
            validation::put(proof, 8, ARS_E_OUT_OF_BUDGET); validation::put(proof, 16, 1);
            return std::fwrite(proof.data(), 1, proof.size(), stdout) == proof.size() ? 0 : 33;
        }
        if (std::wcscmp(argv[1], L"--fault-output") == 0) {
            for (unsigned i = 0; i < 1024; ++i) std::fputs("untrusted", stdout);
            std::fflush(stdout); return 0;
        }
    }
#endif
    if (argc != 2 || std::wcscmp(argv[1], L"--cpu-admit") != 0)
        return 22;
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    validation::Message request{};
    if (std::fread(request.data(), 1, request.size(), stdin) != request.size() ||
        validation::get(request, 0) != magic || validation::get(request, 4) != 1 ||
        validation::get(request, 24) || validation::get(request, 28) ||
        !(validation::get(request, 16) || validation::get(request, 20)))
        return 23;
    const auto length = validation::get(request, 8);
    if (!length || length > input_cap)
        return 23;
    ars_status status = ARS_E_INTERNAL;
    try {
        std::vector<std::uint8_t> bytes(length);
        if (std::fread(bytes.data(), 1, bytes.size(), stdin) != bytes.size() || std::fgetc(stdin) != EOF ||
            validation::checksum({reinterpret_cast<const char *>(bytes.data()), bytes.size()}) !=
                validation::get(request, 12))
            return 23;
        assets::Resource resource(allocate, release, nullptr, 256 * assets::MiB);
        // Destroy every model/parse arena before its allocator context.
        {
            std::shared_ptr<const assets::ValidatedCpuModel> model;
            assets::Limits limits;
            limits.compressed = input_cap;
            status = assets::validate_bundle_checked(bytes, schema_check, nullptr, resource, model, limits);
            if (bool(model) != (status == ARS_OK))
                return 24;
        }
        if (resource.live())
            return 24;
    } catch (const std::bad_alloc &) {
        status = ARS_E_OUT_OF_BUDGET;
    } catch (...) {
        status = ARS_E_INTERNAL;
    }
    validation::Message reply{};
    validation::put(reply, 0, magic);
    validation::put(reply, 4, 1);
    validation::put(reply, 8, static_cast<std::uint32_t>(status));
    validation::put(reply, 16, validation::get(request, 16));
    validation::put(reply, 20, validation::get(request, 20));
    return std::fwrite(reply.data(), 1, reply.size(), stdout) == reply.size() && std::fflush(stdout) == 0 ? 0 : 25;
}
