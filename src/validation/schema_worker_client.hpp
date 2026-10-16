#pragma once
#include "../assets/asset_limits.hpp"
#include "schema_worker_protocol.hpp"
#include <atomic>
#include <string>
#include <string_view>
namespace ars::validation {
struct WorkerConfig {
    // UTF-8 trusted deployment path, never derived from an asset. Exact application,
    // no shell/PATH lookup. Context is deliberately not a public C ABI.
    std::string executable;
    std::uint32_t deadline_ms = max_deadline_ms;
};
struct WorkerResult {
    ars_status status = ARS_E_INTERNAL;
    bool reaped = false;
    std::uint64_t process_id = 0;
};
WorkerResult run_schema_process(const WorkerConfig &, const Message &, std::string_view) noexcept;
class SchemaClient {
    WorkerConfig config_;
    std::atomic_flag active_ = ATOMIC_FLAG_INIT;

  public:
    explicit SchemaClient(WorkerConfig config) : config_(std::move(config)) {}
    ars_status validate(Schema, std::string_view, assets::Resource &,
                        WorkerResult *evidence = nullptr) noexcept;
};
} // namespace ars::validation
