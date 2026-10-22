// Trusted diagnostic pipe. No package parser, model path or public control server.
#include "packet_publication.hpp"
#include "lease_publication.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <fcntl.h>
#include <io.h>
using ars::graphics::Result;
using ars::moc3::PacketPublication;
struct NativeQuery:ars::graphics::d3d11::QueryOps {
    PacketPublication* owner=nullptr;ars::moc3::LeasePublication* lease=nullptr;bool lease_revoke=false,lease_cancel=false;bool revoke_ready=false,delay_once=false;std::uint64_t ready_calls=0;
    HRESULT removed_reason(ID3D11Device* d) override {return d->GetDeviceRemovedReason();}
    HRESULT get_data(ID3D11DeviceContext* c,ID3D11Query* q,BOOL* done) override {
        if(delay_once){delay_once=false;*done=FALSE;return S_FALSE;}
        auto r=c->GetData(q,done,sizeof(*done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(r==S_OK&&*done){++ready_calls;if(revoke_ready){revoke_ready=false;if(owner->revoke()!=ARS_OK)return E_FAIL;}if(lease_revoke){lease_revoke=false;if(lease->revoke()!=ARS_OK)return E_FAIL;}if(lease_cancel){lease_cancel=false;lease->cancel();}}
        return r;
    }
    void flush(ID3D11DeviceContext* c) override {c->Flush();}
};
void save(const std::filesystem::path& folder,const std::vector<std::uint8_t>& rgba){
    {std::ofstream f(folder/"current.rgba",std::ios::binary);f.write(reinterpret_cast<const char*>(rgba.data()),rgba.size());if(!f)throw std::runtime_error("readback write");}
    std::ofstream f(folder/"current.ppm",std::ios::binary);f<<"P6\n512 512\n255\n";
    for(unsigned y=0;y<512;++y)for(unsigned x=0;x<512;++x){auto p=(y*512+x)*4;unsigned bg=((x/16+y/16)%2)?180:220;for(int c=0;c<3;++c)f.put(static_cast<char>(std::min(255u,unsigned(rgba[p+c])+bg*(255-rgba[p+3])/255)));}if(!f)throw std::runtime_error("presenter write");
}
int main(int argc,char** argv){
    if(argc!=3)return 2;auto mode=std::string_view(argv[2]);if(mode!="--warp"&&mode!="--hardware")return 2;
    _setmode(_fileno(stdin),_O_BINARY);std::ios::sync_with_stdio(false);auto folder=std::filesystem::path(argv[1]);std::filesystem::create_directories(folder);
    NativeQuery ops;PacketPublication owner(mode=="--hardware",&ops);ars::moc3::LeasePublication lease(mode=="--hardware",&ops);ops.lease=&lease;ops.owner=&owner;if(owner.open()!=ARS_OK)return 3;
    std::cout<<"READY\n"<<std::flush;
    try{char line_buffer[257]{};unsigned commands=0;while(std::cin.getline(line_buffer,sizeof(line_buffer))){std::string line(line_buffer);
        if(line.size()>256||++commands>10000)ExitProcess(2);std::istringstream in(line);std::string command,extra;in>>command;
        if(command=="LEASE_OPEN"){
            std::uint64_t session=0;if(!(in>>session)||in>>extra)ExitProcess(2);std::cout<<(lease.open(session)==ARS_OK?"OPENED":"DENIED")<<std::endl;
        }else if(command=="LEASE_BEGIN"){
            std::uint64_t size=0;std::uint32_t ms=0;if(!(in>>size>>ms)||in>>extra||size>4096)ExitProcess(2);std::vector<std::uint8_t> body(static_cast<std::size_t>(size));if(!std::cin.read(reinterpret_cast<char*>(body.data()),body.size()))ExitProcess(2);
            std::cout<<(lease.begin(body,ms)==ARS_OK?"PREPARED":"DENIED")<<std::endl;
        }else if(command=="LEASE_STAGE"||command=="LEASE_FAIL"){
            std::uint64_t size=0;if(!(in>>size)||in>>extra||size>ars::moc3::admission::packet_cap)ExitProcess(2);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));if(!std::cin.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))ExitProcess(2);
            std::cout<<(lease.stage(bytes,command=="LEASE_FAIL")==Result::ok?"STAGED":"REJECTED")<<std::endl;
        }else if(command=="LEASE_POLL"){
            auto r=lease.poll();std::cout<<(r==Result::busy?"BUSY":r==Result::ok?"PUBLISHED":"REJECTED")<<std::endl;
        }else if(command=="LEASE_CLOSE"){auto r=lease.close();std::cout<<(r==Result::busy?"BUSY":r==Result::ok?"CLOSED":"REJECTED")<<std::endl;
        }else if(command=="LEASE_CANCEL"){lease.cancel();std::cout<<"CANCELLED"<<std::endl;
        }else if(command=="LEASE_REVOKE"){std::cout<<(lease.revoke()==ARS_OK?"REVOKED":"DENIED")<<std::endl;
        }else if(command=="LEASE_REVOKE_READY"){ops.lease_revoke=true;std::cout<<"ARMED"<<std::endl;
        }else if(command=="LEASE_CANCEL_READY"){ops.lease_cancel=true;std::cout<<"ARMED"<<std::endl;
        }else if(command=="LEASE_STATUS"){
            auto s=lease.state();std::cout<<"STATE "<<s.generation<<' '<<s.host_generation<<' '<<s.epoch<<' '<<s.package<<' '<<s.clock<<' '<<s.pending<<' '<<s.terminal<<' '<<s.device_calls<<' '<<s.creations<<' '<<s.ingress<<' '<<s.core_status<<' '<<ops.ready_calls<<std::endl;
        }else if(command=="LEASE_CHECKPOINT"||command=="LEASE_CORE"){
            auto bytes=command=="LEASE_CORE"?lease.core_bytes():lease.checkpoint();std::ofstream f(folder/(command=="LEASE_CORE"?"current.core":"current.leasecp"),std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!f)ExitProcess(6);f.close();std::cout<<"SAVED"<<std::endl;
        }else if(command=="LEASE_READBACK"){save(folder,lease.read_pixels());std::cout<<"READBACK"<<std::endl;
        }else if(command=="STEP_BEGIN"){
            std::uint64_t seq,nonce,clock;std::uint32_t lease;float value;std::string key;
            if(!(in>>seq>>nonce>>clock>>value>>lease>>key)||in>>extra||key.size()!=64)ExitProcess(2);
            ars::moc3::admission::Digest model{};
            auto digit=[](char c)->unsigned {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;ExitProcess(2);};
            for(unsigned i=0;i<32;++i)model[i]=static_cast<std::uint8_t>((digit(key[2*i])<<4)|digit(key[2*i+1]));
            std::cout<<(owner.begin_step(seq,nonce,clock,value,lease,model)==ARS_OK?"PREPARED":"DENIED")<<std::endl;
        }else if(command=="BEGIN"){
            std::uint64_t seq,nonce,clock;std::uint32_t index,lease;float value;
            if(!(in>>seq>>nonce>>index>>clock>>value>>lease)||in>>extra)ExitProcess(2);
            std::cout<<(owner.begin(seq,nonce,index,clock,value,lease)==ARS_OK?"PREPARED":"DENIED")<<std::endl;
        }else if(command=="STAGE"||command=="STAGE_FAIL"||command=="STEP_STAGE"||command=="STEP_FAIL"){
            std::uint64_t size=0;if(!(in>>size)||in>>extra||size>ars::moc3::admission::packet_cap)ExitProcess(2);
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));if(!std::cin.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))ExitProcess(2);
            auto r=command.starts_with("STEP")?owner.stage_step(bytes,command=="STEP_FAIL"):owner.stage(bytes,command=="STAGE_FAIL");std::cout<<(r==Result::ok?"STAGED":"REJECTED")<<std::endl;
        }else if(command=="POLL"){
            auto r=owner.poll();std::cout<<(r==Result::busy?"BUSY":r==Result::ok?"PUBLISHED":"REJECTED")<<std::endl;
        }else if(command=="CANCEL"){owner.cancel();std::cout<<"CANCELLED"<<std::endl;
        }else if(command=="REVOKE_READY"){ops.revoke_ready=true;std::cout<<"ARMED"<<std::endl;
        }else if(command=="DELAY_ONCE"){ops.delay_once=true;std::cout<<"ARMED"<<std::endl;
        }else if(command=="STATUS"){
            auto s=owner.state();std::cout<<"STATE "<<s.core_generation<<' '<<s.host_generation<<' '<<s.epoch<<' '<<s.physics_cursor<<' '<<s.terminal<<' '<<s.pending<<' '<<s.target_matches_accepted<<' '<<s.device_calls<<' '<<s.gpu_creations<<' '<<ops.ready_calls<<' '<<s.core_status<<' '<<s.core_value<<std::endl;
        }else if(command=="CHECKPOINT"){
            auto& bytes=owner.checkpoint_bytes();std::ofstream f(folder/"current.checkpoint",std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!f)ExitProcess(6);f.close();std::cout<<"CHECKPOINT"<<std::endl;
        }else if(command=="READBACK"){save(folder,owner.read_pixels());std::cout<<"READBACK"<<std::endl;
        }else if(command=="QUIT"){
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);auto r=owner.close();
            while(r==Result::busy&&std::chrono::steady_clock::now()<deadline){std::this_thread::yield();r=owner.close();}
            auto lr=lease.close();while(lr==Result::busy&&std::chrono::steady_clock::now()<deadline){std::this_thread::yield();lr=lease.close();}
            if(r!=Result::ok||lr!=Result::ok)ExitProcess(5);std::cout<<"BYE"<<std::endl;return 0;
        }else ExitProcess(2);
    }}catch(...){ExitProcess(6);}
    // Do not unwind a possible unknown-completion query after malformed transport.
    ExitProcess(7);
}
