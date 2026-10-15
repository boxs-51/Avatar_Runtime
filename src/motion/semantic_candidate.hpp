#pragma once
#include "staged_state.hpp"
namespace ars::motion {
enum class Op { neutral,input,retained,add,scale,store,trigger,output };
struct Node {
    Op op=Op::neutral;
    std::uint32_t left=0,right=0,index=0,source=0;
    float weight=1;
    std::uint64_t due=0;
};
struct Program { std::array<Node,max_nodes> nodes{}; std::uint32_t count=0; };
struct SemanticCandidate {
    StagedState state;
    std::array<Value,max_cells> outputs{};
    std::array<security::OwnedFrame,max_dependencies> inputs{};
    std::array<bool,max_dependencies> present{};
    std::uint32_t input_count=0,channel_count=0;
    ars_status evaluate(const Program &,std::span<const security::SemanticChannel>);
};
}
