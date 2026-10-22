#pragma once
#include "cpu_model_packet.hpp"
#include <memory>
namespace ars::assets {
#ifdef ARS_CPU_ISOLATION_TESTING
void hold_cpu_cleanup_for_test(bool) noexcept;
#endif
class AdmissionCustody {
    struct State;
    std::unique_ptr<State> state_;
    static bool reap_state(State &) noexcept;
    friend ars_status isolated_cpu_load(std::span<const std::uint8_t>,Resource &,
        std::shared_ptr<const ValidatedCpuModel> &,std::unique_ptr<AdmissionCustody> &) noexcept;
  public:
    AdmissionCustody();
    ~AdmissionCustody();
    AdmissionCustody(const AdmissionCustody &)=delete;
    // Only true after OS job emptiness, profile deletion and owned temp cleanup.
    bool reap() noexcept;
    static bool reap_orphan() noexcept;
};
// If pending is nonnull on return, caller must retain runtime custody and reap
// during management/close. It must not discard it or permit finish_close.
ars_status isolated_cpu_load(std::span<const std::uint8_t>,Resource &,
    std::shared_ptr<const ValidatedCpuModel> &,std::unique_ptr<AdmissionCustody> &) noexcept;
} // namespace ars::assets
