#include "public_resolution.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace ars::motion {
namespace {
unsigned width(std::uint32_t kind) {
    return kind==ARS_VALUE_VEC2?2:kind==ARS_VALUE_VEC3?3:kind==ARS_VALUE_QUAT_XYZW?4:1;
}
Value value(std::array<float,4> v,std::uint16_t mask=0) { return {v[0],mask,{v[1],v[2],v[3]}}; }
}
ars_status PublicResolution::evaluate(SemanticCandidate &candidate,std::span<const security::SemanticChannel> catalog) {
    if(count>max_dependencies || count!=candidate.input_count || catalog.size()>max_cells) return ARS_E_INVALID_ARGUMENT;
    candidate.state={}; candidate.channel_count=static_cast<std::uint32_t>(catalog.size()); candidate.outputs={};
    for(std::uint32_t i=0;i<count;++i) {
        const auto &source=sources[i]; if(!source.cache || !source.epoch) return ARS_E_INVALID_ARGUMENT;
        next[i]={};
        const auto &old=source.cache->frame;
        if(security::same(old.source.id,source.id.id) && old.epoch==source.epoch) next[i]=*source.cache;
        if(candidate.present[i]) {
            const auto &fresh=candidate.inputs[i];
            if(!security::same(fresh.source.id,source.id.id) || fresh.epoch!=source.epoch) return ARS_E_REVOKED;
            if(next[i].frame.count && !security::same(fresh.character.id,next[i].frame.character.id)) return ARS_E_SCOPE_DENIED;
            next[i].frame.source=fresh.source; next[i].frame.character=fresh.character; next[i].frame.epoch=fresh.epoch;
            for(std::uint32_t c=0;c<fresh.count;++c) {
                const auto &channel=fresh.channels[c]; if(channel.index>=catalog.size()) return ARS_E_INVALID_FRAME;
                if(channel.kind==ARS_VALUE_TRIGGER) continue;
                auto &merged=next[i].frame; std::uint32_t at=0;
                while(at<merged.count && merged.channels[at].index!=channel.index) ++at;
                if(at==merged.count) { if(merged.count==max_cells) return ARS_E_OUT_OF_BUDGET; ++merged.count; }
                merged.channels[at]=channel; next[i].confidence[channel.index]=fresh.confidence;
            }
        }
    }
    for(std::uint32_t c=0;c<catalog.size();++c) {
        const auto kind=catalog[c].kind; auto result=catalog[c].neutral;
        if(kind==ARS_VALUE_TRIGGER) result={};
        std::int32_t priority=std::numeric_limits<std::int32_t>::min(); double total=0;
        std::array<double,4> sum{}; std::array<float,4> reference{}; bool reference_set=false;
        std::uint16_t dependencies=0; bool bool_tie=false; ars_handle_id tie_id{};
        for(std::uint32_t i=0;i<count;++i) {
            const security::OwnedChannel *channel=nullptr; const security::OwnedFrame *frame=nullptr; float confidence=0;
            if(kind==ARS_VALUE_TRIGGER) {
                if(!candidate.present[i]) continue;
                frame=&candidate.inputs[i]; confidence=frame->confidence;
            } else { frame=&next[i].frame; confidence=next[i].confidence[c]; }
            for(std::uint32_t j=0;j<frame->count;++j) if(frame->channels[j].index==c) { channel=&frame->channels[j]; break; }
            if(!std::isfinite(confidence) || confidence<0 || confidence>1) return ARS_E_INVALID_FRAME;
            if(!channel || confidence==0) continue;
            if(channel->kind!=kind || !security::valid_value(kind,channel->value)) return ARS_E_INVALID_FRAME;
            const auto p=sources[i].priority; if(p<priority) continue;
            if(p>priority) { priority=p;total=0;sum={};dependencies=0;reference_set=false;tie_id={};bool_tie=false; }
            std::uint16_t mask=0; auto status=candidate.state.provenance.include({frame->character,frame->source,frame->epoch},&mask);
            if(status!=ARS_OK) return status;
            dependencies=static_cast<std::uint16_t>(dependencies|mask);
            auto v=channel->value;
            if(kind==ARS_VALUE_QUAT_XYZW) {
                if(!reference_set) { reference=v;reference_set=true; }
                double dot=0;for(unsigned j=0;j<4;++j)dot+=double(reference[j])*v[j];
                if(dot<0)for(auto &component:v)component=-component;
            }
            if(!tie_id.runtime_uid || sources[i].id.id.slot<tie_id.slot ||
               (sources[i].id.id.slot==tie_id.slot && sources[i].id.id.generation<tie_id.generation)) {
                tie_id=sources[i].id.id;bool_tie=v[0]!=0;
            }
            for(unsigned j=0;j<width(kind);++j) sum[j]+=double(v[j])*confidence;
            total+=confidence;
        }
        if(total>0) {
            if(kind==ARS_VALUE_BOOL) result[0]=sum[0]*2==total ? (bool_tie?1.f:0.f) : sum[0]*2>total?1.f:0.f;
            else if(kind==ARS_VALUE_TRIGGER) result[0]=sum[0]>0?1.f:0.f;
            else {
                for(unsigned j=0;j<width(kind);++j) result[j]=static_cast<float>(sum[j]/total);
                if(kind==ARS_VALUE_QUAT_XYZW) {
                    double norm=0;for(auto x:result)norm+=double(x)*x;
                    if(norm<1e-12) result=catalog[c].neutral;
                    else { const double inverse=1/std::sqrt(norm);for(auto &x:result)x=static_cast<float>(x*inverse); }
                }
                const auto &bound=constraints[c]; bool outside=false;
                for(unsigned j=0;j<width(kind);++j) {
                    outside|=(bound.has_minimum && result[j]<bound.minimum)||(bound.has_maximum && result[j]>bound.maximum);
                    if(kind!=ARS_VALUE_QUAT_XYZW) {
                        if(bound.has_minimum)result[j]=std::max(result[j],bound.minimum);
                        if(bound.has_maximum)result[j]=std::min(result[j],bound.maximum);
                    }
                }
                if(kind==ARS_VALUE_QUAT_XYZW && outside)result=catalog[c].neutral;
            }
        }
        if(!security::valid_value(kind,result)) return ARS_E_INVALID_FRAME;
        candidate.outputs[c]=value(result,dependencies);
        candidate.state.cells[c]=candidate.outputs[c];
    }
    // Drop provenance from losing priority bands and remap surviving masks.
    candidate.state.compact();
    for(std::uint32_t c=0;c<catalog.size();++c) candidate.outputs[c]=candidate.state.cells[c];
    return ARS_OK;
}
void PublicResolution::commit(void *opaque) noexcept {
    auto &resolution=*static_cast<PublicResolution *>(opaque);
    for(std::uint32_t i=0;i<resolution.count;++i)*resolution.sources[i].cache=resolution.next[i];
}
} // namespace ars::motion
