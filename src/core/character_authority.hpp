#pragma once
#include "handle_registry.hpp"
#include "../security/authority_types.hpp"
#include <atomic>
#include <mutex>
#include <memory>

namespace ars::security { class SourceAuthority; }
namespace ars::motion { struct Snapshot; }
namespace ars::core {
// Context storage is fixed; a direct registry pin and runtime lease protect it.
class ObjectLease {
    RegistryLease registry_;
    std::atomic<std::uint32_t> *users_=nullptr;
    friend class ars::security::SourceAuthority;
public:
    ObjectLease()=default;
    ObjectLease(const ObjectLease &)=delete;
    ObjectLease &operator=(const ObjectLease &)=delete;
    ObjectLease(ObjectLease &&) noexcept;
    ObjectLease &operator=(ObjectLease &&) noexcept;
    ~ObjectLease();
    void reset() noexcept;
    void *payload() const noexcept { return registry_.payload(); }
};
struct CharacterAuthority {
    std::mutex pose;
    std::atomic<std::uint32_t> users{0};
    bool allocated=false,alive=false;
    // Detached by full record identity before reuse, then freed outside locks.
    std::shared_ptr<const void> cpu_owner;
    ars_character_h id{};
    std::uint32_t count=0;
    motion::Snapshot *snapshot=nullptr;
    std::uint64_t pose_generation=0;
    bool pose_exhausted=false;
    std::array<security::SemanticChannel,security::max_channels> catalog{};
};
ars_status validate_catalog(std::span<const security::SemanticChannel>) noexcept;
}
