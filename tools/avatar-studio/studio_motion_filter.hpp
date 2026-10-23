#pragma once
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
namespace ars::experimental::lps {
struct MotionXY {float x=0,y=0;};
class MotionFilter {
 std::vector<MotionXY> accepted_;
 std::uint64_t revision_=0;
public:
 struct Candidate {std::vector<MotionXY> positions;std::uint64_t revision=0;};
 void clear(){accepted_.clear();++revision_;}
 bool prepare(const std::vector<MotionXY>&source,double seconds,double response,Candidate&out,const std::vector<unsigned char>* pinned_mask=nullptr)const{
  if(!std::isfinite(seconds)||seconds<=0||seconds>.1||
     !std::isfinite(response)||response<0||response>1)return false;
  Candidate next;next.revision=revision_;next.positions=source;
  const double alpha=response==0?1:-std::expm1(-seconds/response);
  for(std::size_t i=0;i<source.size();++i){
   const auto &v=source[i];
   if(!std::isfinite(v.x)||!std::isfinite(v.y))return false;
   if(pinned_mask&&i<pinned_mask->size()&&(*pinned_mask)[i])continue;
   if(i<accepted_.size()){
    next.positions[i].x=static_cast<float>(accepted_[i].x+(v.x-accepted_[i].x)*alpha);
    next.positions[i].y=static_cast<float>(accepted_[i].y+(v.y-accepted_[i].y)*alpha);
   }
  }
  out=std::move(next);return true;
 }
 bool accept(Candidate&&next){
  if(next.revision!=revision_)return false;
  for(const auto &v:next.positions)if(!std::isfinite(v.x)||!std::isfinite(v.y))return false;
  accepted_=std::move(next.positions);++revision_;return true;
 }
};
}
