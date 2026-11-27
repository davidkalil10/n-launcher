// SPDX-License-Identifier: GPL-2.0-or-later
#include "gui.hpp"
#include <cstdio>
#include <filesystem>
using namespace SwitchFrontend::Ui;
static void save(nlauncher::Menu& menu,Canvas& canvas,const std::string& out){
    menu.draw(canvas);auto* f=std::fopen(out.c_str(),"wb");if(!f)throw std::runtime_error("Preview output unavailable");
    std::fprintf(f,"P6\n%d %d\n255\n",canvas.Width(),canvas.Height());
    for(int i=0;i<canvas.Width()*canvas.Height();++i){auto p=canvas.Data()[i];unsigned char rgb[]={static_cast<unsigned char>(p),static_cast<unsigned char>(p>>8),static_cast<unsigned char>(p>>16)};std::fwrite(rgb,1,3,f);}
    std::fclose(f);
}
int main(int argc,char** argv){
    if(argc!=3)return 2;
    Flow::SetCardRoot(argv[1]);std::filesystem::create_directories(argv[2]);
    nlauncher::Menu menu;Canvas canvas;menu.start();
    save(menu,canvas,std::string(argv[2])+"/library.ppm");
    nlauncher::Keys settings;settings.l=true;menu.update(settings);save(menu,canvas,std::string(argv[2])+"/settings.ppm");settings={};settings.b=true;menu.update(settings);
    nlauncher::Keys key;key.y=true;menu.update(key);save(menu,canvas,std::string(argv[2])+"/options.ppm");
    key={};key.a=true;menu.update(key);save(menu,canvas,std::string(argv[2])+"/memory.ppm");
    key={};key.b=true;menu.update(key);
    key={};key.b=true;menu.update(key);key={};key.minus=true;menu.update(key);save(menu,canvas,std::string(argv[2])+"/folder.ppm");
    menu.report("Preparando o jogo","Conferindo módulos e integridade dos arquivos.\nO NUP original permanece intacto.",true,0.42f);
    save(menu,canvas,std::string(argv[2])+"/progress.ppm");menu.offer_update("0.0.7",20000000);save(menu,canvas,std::string(argv[2])+"/update.ppm");return 0;
}
