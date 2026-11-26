// SPDX-License-Identifier: GPL-2.0-or-later
// N Launcher frontend. Uses the nx2-flow/Toronja software canvas; upstream notices are preserved.
#include "gui.hpp"
#include "images.h"
#include "nx2link/version.hpp"
#include <algorithm>
#include <cstdio>
#include <sstream>

namespace nlauncher {
using namespace SwitchFrontend::Ui;
namespace {
const u32 bg=MakeColor(12,17,25),panel=MakeColor(22,29,40),line=MakeColor(43,54,70);
const u32 white=MakeColor(238,244,250),muted=MakeColor(144,162,184),accent=MakeColor(80,226,184);
void image(Canvas& c,const Image& i,int x,int y,int w,int h){
    if(i.Empty())return;
    for(int dy=0;dy<h;++dy)for(int dx=0;dx<w;++dx)
        c.Data()[(y+dy)*c.Width()+x+dx]=i.px[(dy*i.h/h)*i.w+dx*i.w/w];
}
std::string parent(std::string p){auto n=p.find_last_of('/');return n==0?"/":p.substr(0,n);}
}
void Menu::text(Canvas& c,int x,int y,std::string_view s,int size,u32 color){font.Draw(c,x,y,s,size,color?color:white);}
std::string Menu::fit(std::string s,int width,int size){
    if(font.Measure(s,size)<=width)return s;
    while(!s.empty()&&font.Measure(s+"…",size)>width){auto pos=s.size()-1;while(pos>0&&(static_cast<unsigned char>(s[pos])&0xc0)==0x80)--pos;s.resize(pos);}
    return s+"…";
}
void Menu::save(){Flow::WriteSettings(Flow::kMenuCfg,{{"games",folder}});}
void Menu::scan(){games=Flow::ScanGames(folder);selected=std::min(selected,std::max(0,int(games.size())-1));icons.clear();}
void Menu::start(){auto cfg=Flow::ReadSettings(Flow::kMenuCfg);if(cfg.count("games")&&Flow::IsFolder(cfg["games"]))folder=cfg["games"];scan();}
void Menu::folders(){dirs=Flow::ListFolders(picker);dirs.erase(std::remove_if(dirs.begin(),dirs.end(),[](const std::string& s){auto tail=s.size()>=4?s.substr(s.size()-4):"";std::transform(tail.begin(),tail.end(),tail.begin(),[](unsigned char c){return std::tolower(c);});return tail==".nup";}),dirs.end());folder_cursor=0;}
std::vector<std::string> Menu::message_lines(){
    std::vector<std::string> lines;std::istringstream input(detail);std::string paragraph;
    while(std::getline(input,paragraph)){auto wrapped=WrapLines(font,paragraph,19,720);if(wrapped.empty())lines.emplace_back();else lines.insert(lines.end(),wrapped.begin(),wrapped.end());}
    return lines;
}
void Menu::report(std::string t,std::string d,bool b,float f){title=std::move(t);detail=std::move(d);if(!working||!b)cancel=false;working=b;progress=f;message_scroll=0;screen=Screen::Message;}
void Menu::finish_job(std::string t,std::string d){report(std::move(t),std::move(d));}
Request Menu::update(Keys k){
    if(working){if(k.b)cancel=true;return {};}
    if(screen==Screen::Message){
        if(k.a||k.b)screen=Screen::Library;
        if(k.up&&message_scroll>0)--message_scroll;
        auto count=message_lines().size();if(k.down&&message_scroll+9<count)++message_scroll;
        return {};
    }
    if(screen==Screen::Picker){
        if(k.up&&folder_cursor>0)--folder_cursor;
        if(k.down&&folder_cursor+1<int(dirs.size()))++folder_cursor;
        if(k.a&&!dirs.empty()){picker+=(picker=="/"?"":"/")+dirs[folder_cursor];folders();}
        if(k.b){if(picker=="/")screen=Screen::Library;else{picker=parent(picker);folders();}}
        if(k.y){folder=picker;save();scan();screen=Screen::Library;}
        return {};
    }
    if(screen==Screen::Options){
        if(k.b){screen=Screen::Library;return {};}
        if(k.up&&cursor>0)--cursor;
        if(k.down&&cursor<4)++cursor;
        if(k.a){screen=Screen::Library;return {cursor==0?Action::Prepare:cursor==1?Action::Inspect:cursor==2?Action::Probe:cursor==3?Action::Logs:Action::None,games[selected].path};}
        return {};
    }
    if(k.plus)return {Action::Exit,{}};
    if(k.minus){picker=Flow::IsFolder(folder)?folder:"/";folders();screen=Screen::Picker;return {};}
    if(k.x){scan();return {};}
    if(games.empty())return {};
    if(k.right&&selected+1<int(games.size()))++selected;
    if(k.left&&selected>0)--selected;
    if(k.down)selected=std::min(selected+3,int(games.size())-1);
    if(k.up)selected=std::max(0,selected-3);
    if(k.y){screen=Screen::Options;cursor=0;}
    if(k.a){if(!games[selected].error.empty()){report("Não foi possível abrir",games[selected].error);return {};}return {Action::Launch,games[selected].path};}
    return {};
}
void Menu::draw(Canvas& c){
    ++frames;
    c.Clear(bg);
    c.FillRect(0,0,86,720,MakeColor(16,23,32));
    c.FillRoundRect(19,27,48,48,14,accent);text(c,32,61,"N",32,bg);
    c.FillRoundRect(20,132,47,47,12,MakeColor(36,72,69));
    for(int dx:{0,12})for(int dy:{0,12})c.FillRoundRect(32+dx,144+dy,9,9,2,accent);
    text(c,28,687,"SD",16,muted);
    text(c,116,53,"N Launcher",29);text(c,116,80,"Sua biblioteca, direto no console.",16,muted);
    c.FillRoundRect(1040,31,202,34,17,panel);text(c,1058,54,"SWITCH 2  /  " NX2LINK_VERSION,14,accent);
    c.FillRect(115,101,1128,1,line);
    text(c,116,139,"BIBLIOTECA",13,accent);
    text(c,1050,139,std::to_string(games.size())+" jogos encontrados",14,muted);
    if(games.empty()){
        text(c,155,260,"Sua biblioteca começa aqui",36);
        text(c,155,305,"Escolha a pasta onde estão seus jogos NUP.",22,muted);
        text(c,155,344,"Arquivos completos e pastas com partes 00, 01… são aceitos.",18,muted);
        c.FillRoundRect(155,387,260,52,12,accent);text(c,174,421,"−  Escolher pasta",22,bg);
    }else{
        const int page=selected/6*6;
        for(int i=page;i<std::min(page+6,int(games.size()));++i){
            auto& g=games[i];const int x=116+(i-page)%3*380,y=162+(i-page)/3*220;
            const bool focus=i==selected;
            c.FillRoundRect(x-3,y-3,365,206,17,focus?accent:line);
            c.FillRoundRect(x,y,359,200,14,panel);
            auto [it,added]=icons.try_emplace(g.path);
            if(added&&!g.id.empty())Images::LoadFile(Flow::OnCard(std::string(Flow::kCache)+"/"+g.id+".jpg"),it->second,256);
            c.FillRoundRect(x+14,y+14,130,130,12,MakeColor(35,48,63));
            if(!it->second.Empty())image(c,it->second,x+18,y+18,122,122);
            else{text(c,x+58,y+95,"N",49,accent);}
            text(c,x+158,y+40,g.error.empty()?"PRONTO PARA VERIFICAR":"VERIFICAR ARQUIVO",10,g.error.empty()?accent:MakeColor(255,171,123));
            auto lines=WrapLines(font,g.title,21,184);int ty=y+73;
            for(std::size_t j=0;j<std::min<std::size_t>(3,lines.size());++j){text(c,x+158,ty,fit(lines[j],184,21),21);ty+=25;}
            text(c,x+158,y+153,Flow::HumanSize(g.size),17,muted);
            text(c,x+16,y+180,g.parts>1?"NUP DIVIDIDO · "+std::to_string(g.parts)+" PARTES":"NUP COMPLETO",12,accent);
            text(c,x+202,y+180,g.id.empty()?"ID indisponível":g.id,11,muted);
        }
    }
    c.FillRoundRect(116,620,1125,37,9,panel);text(c,132,644,fit(folder,1080,15),15,muted);
    text(c,117,692,"A  Jogar",19,accent);text(c,258,692,"Y  Opções",18);text(c,430,692,"X  Atualizar",18,muted);
    text(c,641,692,"−  Pasta",18,muted);text(c,813,692,"Direcional  Navegar",18,muted);text(c,1123,692,"+  Sair",18,muted);
    if(screen!=Screen::Library){
        c.FillRect(0,0,1280,720,MakeColor(0,0,0,185));
        c.FillRoundRect(238,126,805,476,22,panel);
        if(screen==Screen::Picker){
            text(c,270,171,"Onde estão seus jogos?",28);text(c,270,207,fit(picker,730,16),16,accent);
            if(dirs.empty())text(c,273,274,"Nenhuma subpasta. Y seleciona esta pasta.",19,muted);
            int first=folder_cursor/6*6;
            for(int i=first;i<std::min(first+6,int(dirs.size()));++i){int y=229+(i-first)*47;c.FillRoundRect(265,y,750,42,8,i==folder_cursor?MakeColor(36,72,69):panel);text(c,282,y+28,fit(dirs[i],705,19),19,i==folder_cursor?accent:white);}
            text(c,274,574,"A  Abrir   ·   B  Voltar   ·   Y  Usar esta pasta",18,accent);
        }else if(screen==Screen::Options){
            text(c,270,175,"Opções do jogo",28);text(c,270,210,fit(games[selected].title,730,18),18,muted);
            const char* rows[]={"Preparar e verificar jogo","Conferir integridade do índice","Testar carregador da sessão","Ver registros da última operação","Voltar à biblioteca"};
            for(int i=0;i<5;++i){int y=234+i*56;c.FillRoundRect(265,y,750,48,10,cursor==i?MakeColor(36,72,69):bg);text(c,284,y+32,rows[i],21,cursor==i?accent:white);}
            text(c,272,564,"A  Selecionar   ·   B  Voltar",18,muted);
        }else{
            text(c,270,180,fit(title,730,28),28);
            auto lines=message_lines();int y=228;auto first=working&&lines.size()>9?lines.size()-9:message_scroll;
            for(std::size_t j=first;j<std::min(first+9,lines.size());++j){text(c,272,y,lines[j],19,muted);y+=29;}
            if(working){c.FillRoundRect(272,527,732,8,4,line);
                int x=progress<0?272+int((frames%90)/90.0f*530):272;
                int width=progress<0?202:int(732*std::clamp(progress,0.0f,1.0f));
                c.FillRoundRect(x,527,width,8,4,accent);text(c,272,574,cancel?"Cancelando na próxima etapa segura…":"B  Cancelar ao concluir a etapa atual",17,muted);}
            else text(c,272,574,lines.size()>9?"A / B  Voltar   ·   Direcional  Rolar texto":"A / B  Voltar à biblioteca",18,accent);
        }
    }
}
}
