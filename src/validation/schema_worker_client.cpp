#include "schema_worker_client.hpp"
#include <filesystem>
namespace ars::validation {
ars_status SchemaClient::validate(Schema kind, std::string_view input, assets::Resource &resource,
                                  WorkerResult *evidence) noexcept {
    if (evidence)
        *evidence = {};
    try {
        if (input.size() > max_request)
            return ARS_E_OUT_OF_BUDGET;
        if ((kind != Schema::manifest && kind != Schema::rig) || input.empty() ||
            config_.executable.size() > 32760 || config_.executable.find('\0') != std::string::npos ||
            !std::filesystem::path(config_.executable).is_absolute() || config_.deadline_ms == 0 ||
            config_.deadline_ms > max_deadline_ms)
            return ARS_E_INVALID_ARGUMENT;
        if (active_.test_and_set())
            return ARS_E_BUSY;
        struct Guard {
            std::atomic_flag &flag;
            ~Guard() { flag.clear(); }
        } guard{active_};
        // Includes the worker hard cap and bounded parent IPC/thread workspace.
        assets::Reservation reservation(resource, worker_memory + 2 * assets::MiB);
        static std::atomic<std::uint64_t> counter{1};
        auto nonce = counter.load();
        do {
            if (nonce == UINT64_MAX)
                return ARS_E_INTERNAL;
        } while (!counter.compare_exchange_weak(nonce, nonce + 1));
        Message request{};
        put(request, 0, schema_magic);
        put(request, 4, schema_protocol);
        put(request, 8, static_cast<std::uint32_t>(kind));
        put(request, 12, static_cast<std::uint32_t>(input.size()));
        put(request, 16, static_cast<std::uint32_t>(nonce));
        put(request, 20, static_cast<std::uint32_t>(nonce >> 32));
        put(request, 24, checksum(input));
        const auto result = run_schema_process(config_, request, input);
        if (evidence)
            *evidence = result;
        return result.status;
    } catch (const assets::Failure &f) {
        return f.status;
    } catch (const std::bad_alloc &) {
        return ARS_E_OUT_OF_BUDGET;
    } catch (...) {
        return ARS_E_INTERNAL;
    }
}
} // namespace ars::validation
