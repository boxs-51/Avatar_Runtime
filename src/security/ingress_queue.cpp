#include "ingress_queue.hpp"

namespace ars::security {
ars_status IngressQueue::append(const OwnedFrame &frame) noexcept {
    if(fail_append_) return ARS_E_OUT_OF_BUDGET;
    if(count_>=limit_) return ARS_E_QUEUE_FULL;
    frames_[(head_+count_)%queue_capacity]=frame; ++count_; return ARS_OK;
}
bool IngressQueue::pop(OwnedFrame *out) noexcept {
    if(!out || !count_) return false;
    *out=frames_[head_]; head_=(head_+1)%queue_capacity; --count_; return true;
}
}
