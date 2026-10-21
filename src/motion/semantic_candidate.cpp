#include "semantic_candidate.hpp"
#include "public_resolution.hpp"
#include <cmath>
namespace ars::motion {
ars_status SemanticCandidate::evaluate(const Program &program,std::span<const security::SemanticChannel> catalog) {
    if(program.public_resolution) return program.public_resolution->evaluate(*this,catalog);
    if(program.count>max_nodes || catalog.size()>max_cells) return ARS_E_OUT_OF_BUDGET;
    channel_count=static_cast<std::uint32_t>(catalog.size());
    for(std::uint32_t i=0;i<channel_count;++i) outputs[i]={catalog[i].neutral[0],0};
    std::array<Value,max_nodes> values{};
    for(std::uint32_t i=0;i<program.count;++i) {
        const auto &node=program.nodes[i]; Value value{};
        if(node.op!=Op::neutral && node.op!=Op::input && node.op!=Op::retained && node.left>=i) return ARS_E_INVALID_ARGUMENT;
        switch(node.op) {
        case Op::neutral:
            if(node.index>=channel_count) return ARS_E_INVALID_ARGUMENT;
            value={catalog[node.index].neutral[0],0}; break;
        case Op::input: {
            if(node.source>=input_count || node.index>=channel_count || catalog[node.index].kind!=ARS_VALUE_SCALAR) return ARS_E_INVALID_ARGUMENT;
            value={catalog[node.index].neutral[0],0};
            if(present[node.source]) {
                const auto &frame=inputs[node.source];
                for(std::uint32_t c=0;c<frame.count;++c) if(frame.channels[c].index==node.index) {
                    std::uint16_t mask=0;
                    auto status=state.provenance.include({frame.character,frame.source,frame.epoch},&mask);
                    if(status!=ARS_OK) return status;
                    value={frame.channels[c].value[0],mask}; break;
                }
            }
            break;
        }
        case Op::retained:
            if(node.index>=max_cells) return ARS_E_OUT_OF_BUDGET;
            value=state.cells[node.index]; break;
        case Op::add:
            if(node.right>=i) return ARS_E_INVALID_ARGUMENT;
            value=add(values[node.left],values[node.right]); break;
        case Op::scale:
            if(!std::isfinite(node.weight)) return ARS_E_INVALID_ARGUMENT;
            value=scale(values[node.left],node.weight); break;
        case Op::store:
            if(node.index>=max_cells) return ARS_E_OUT_OF_BUDGET;
            value=values[node.left]; state.cells[node.index]=value; break;
        case Op::trigger:
            if(state.trigger_count==max_triggers) return ARS_E_OUT_OF_BUDGET;
            value=values[node.left]; state.triggers[state.trigger_count++]={value,node.due}; break;
        case Op::output:
            if(node.index>=channel_count || catalog[node.index].kind!=ARS_VALUE_SCALAR) return ARS_E_INVALID_ARGUMENT;
            value=values[node.left]; outputs[node.index]=value; break;
        default: return ARS_E_INVALID_ARGUMENT;
        }
        if(!std::isfinite(value.scalar)) return ARS_E_INVALID_FRAME;
        values[i]=value;
    }
    return ARS_OK;
}
}
