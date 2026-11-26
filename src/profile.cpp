// SPDX-License-Identifier: MIT
#include "nx2link/profile.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>
namespace nx2link {
bool valid_game_memory(uint32_t n){return !n||(n>=2048&&n<=8992);}
bool valid_loader_memory(uint32_t n){return !n||(n>=128&&n<=2048);}
uint32_t parse_memory(const std::string& s,bool game){
    if(s.empty())return 0;
    if(s.size()>5||s.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Perfil de memória inválido");
    auto n=uint32_t(std::stoul(s));if(!(game?valid_game_memory(n):valid_loader_memory(n)))throw std::runtime_error("Reserva de memória fora do limite suportado");return n;
}
uint32_t step_memory(uint32_t n,int dir,bool game){
    std::vector<uint32_t> choices{0};auto low=game?2048u:128u,high=game?8992u:2048u,step=game?128u:64u;
    for(auto v=low;v<=high;v+=step)choices.push_back(v);
    if(choices.back()!=high)choices.push_back(high);
    auto it=std::find(choices.begin(),choices.end(),n);size_t index=it==choices.end()?0:size_t(it-choices.begin());
    return choices[(index+choices.size()+(dir>0?1:-1))%choices.size()];
}
std::string launch_arguments(const std::string& loader,const std::string& input,const std::string& menu,uint32_t room){
    if(!valid_loader_memory(room))throw std::runtime_error("Reserva do carregador inválida");
    for(const auto* s:{&loader,&input,&menu})if(s->empty()||s->find_first_of("\"\r\n")!=std::string::npos)throw std::runtime_error("Caminho inválido para iniciar o jogo");
    std::string args="\""+loader+"\" \""+input+"\" \"menu="+menu+"\"";
    // Numeric and unquoted: the upstream early-heap parser consumes the raw argv string.
    if(room)args+=" room="+std::to_string(room);
    return args;
}
}
