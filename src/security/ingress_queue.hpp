#pragma once
#include "authority_types.hpp"

namespace ars::security {
class IngressQueue {
    std::array<OwnedFrame,queue_capacity> frames_{};
    std::uint32_t head_=0,count_=0,limit_=queue_capacity;
    bool fail_append_=false;
    friend struct ars::core::ComponentFixture;
public:
    ars_status append(const OwnedFrame &) noexcept;
    bool pop(OwnedFrame *) noexcept;
    void clear() noexcept { head_=count_=0; }
    std::uint32_t size() const noexcept { return count_; }
};
}
