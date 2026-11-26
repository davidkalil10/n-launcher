// SPDX-License-Identifier: MIT
#include "nx2link/layout.hpp"
#include <fstream>
#include <vector>
#include <stdexcept>
namespace nx2link {
namespace fs=std::filesystem;
namespace {
bool data_entry(const fs::path& p){auto n=p.filename().string();if(n.size()>=4&&n.substr(n.size()-4)==".nro")return false;return true;}
std::string replace_prefix(std::string text){
    for(const auto& before:{std::string("sdmc:/switch/nx2-link"),std::string("/switch/nx2-link")})if(text==before||text.rfind(before+"/",0)==0){auto after=before;after.replace(after.find("nx2-link"),8,"n-launcher");text=after+text.substr(before.size());break;}
    return text;
}
void rewrite_menu_path(const fs::path& root){
    auto file=root/"menu.cfg";std::ifstream in(file);if(!in)return;std::string output,line;bool changed=false;
    while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.rfind("games=",0)==0){auto fixed=replace_prefix(line.substr(6));changed|=fixed!=line.substr(6);line="games="+fixed;}output+=line+'\n';}
    in.close();if(!changed)return;
    auto temp=root/"menu.cfg.migration";std::ofstream out(temp,std::ios::trunc);out<<output;out.close();if(!out)throw std::runtime_error("Não foi possível atualizar o caminho da biblioteca");
    auto previous=root/"menu.cfg.before-rename";unsigned index=0;while(fs::exists(previous)&&index<100)previous=root/("menu.cfg.before-rename."+std::to_string(++index));if(fs::exists(previous))throw std::runtime_error("Muitas configurações anteriores da migração; dados preservados");
    fs::rename(file,previous);try{fs::rename(temp,file);}catch(...){fs::rename(previous,file);throw;}
}
}
Migration migrate_layout(const fs::path& sd,const std::string& executable){
    const auto old=sd/"switch/nx2-link",now=sd/"switch/n-launcher";Migration result{executable,false};
    if(fs::exists(old)){
        if(!fs::exists(now)){fs::rename(old,now);result.moved=true;}
        else{
            std::vector<fs::path> entries;
            for(const auto& item:fs::directory_iterator(old))if(data_entry(item.path())){
                if(fs::exists(now/item.path().filename()))throw std::runtime_error("Há dados em nx2-link e n-launcher com o mesmo nome. As duas versões foram preservadas. Organize as pastas antes de continuar.");
                entries.push_back(item.path());
            }
            for(const auto& entry:entries){fs::rename(entry,now/entry.filename());result.moved=true;}
        }
        if(executable.rfind("sdmc:/switch/nx2-link/",0)==0){
            auto relative=fs::u8path(executable.substr(std::string("sdmc:/switch/nx2-link/").size()));
            if(fs::exists(old/relative)){if(fs::exists(now/relative))throw std::runtime_error("Existem duas cópias do aplicativo com o mesmo nome. Abra a versão de n-launcher para continuar.");fs::rename(old/relative,now/relative);}
            result.executable=replace_prefix(executable);
        }
    }
    if(fs::exists(now))rewrite_menu_path(now);
    return result;
}
}
