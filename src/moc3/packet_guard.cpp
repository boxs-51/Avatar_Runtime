#include "drawable_packet.hpp"
#include <charconv>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <vector>
int main(int argc,char** argv){
    if(argc!=2&&argc!=3)return 2;
    bool sequence=argc==3&&std::string_view(argv[2])=="--sequence";if(argc==3&&!sequence)return 2;std::uint64_t nonce=0;auto end=argv[1]+std::char_traits<char>::length(argv[1]);
    auto parsed=std::from_chars(argv[1],end,nonce);if(parsed.ec!=std::errc{}||parsed.ptr!=end||!nonce)return 2;
    _setmode(_fileno(stdin),_O_BINARY);std::vector<std::uint8_t> data;char chunk[16384];
    while(std::cin){std::cin.read(chunk,sizeof(chunk));auto n=std::cin.gcount();if(std::size_t(n)>ars::moc3::admission::packet_cap-data.size()){std::cout<<"REJECT GPU_CREATIONS=0\n";return 3;}data.insert(data.end(),chunk,chunk+n);}
    if(!std::cin.eof()||std::cin.bad()){std::cout<<"REJECT GPU_CREATIONS=0\n";return 3;}
    if(sequence){ars::moc3::admission::VerifiedSequence candidate;
        if(!ars::moc3::admission::decode_sequence(data,nonce,candidate)){std::cout<<"REJECT GPU_CREATIONS=0\n";return 3;}
        std::cout<<"SEQUENCE_VALIDATED GPU_CREATIONS=0 PHYSICS=NOT_SIMULATED\n";return 0;}
    ars::moc3::admission::VerifiedScene scene;
    if(!ars::moc3::admission::decode(data,nonce,scene)){std::cout<<"REJECT GPU_CREATIONS=0\n";return 3;}
    std::cout<<"VALIDATED GPU_CREATIONS=0 PHYSICS="<<((scene.flags()&3)==3?"VALIDATED_NOT_SIMULATED":"ABSENT")<<'\n';return 0;
}
