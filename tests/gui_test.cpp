// SPDX-License-Identifier: GPL-2.0-or-later
#include "gui.hpp"
#include <cassert>
#include <iostream>
int main(int argc,char** argv){
    assert(argc==2);Flow::SetCardRoot(argv[1]);
    auto games=Flow::ScanGames(Flow::kDefaultGames);assert(games.size()==6);unsigned split=0;bool accented=false;
    for(const auto& game:games){assert(game.error.empty());assert(!game.id.empty());split+=game.parts==2;accented|=game.title=="Café Nebula";}
    assert(split==2&&accented);
    nlauncher::Menu menu;menu.start();nlauncher::Keys keys;keys.y=true;menu.update(keys);keys={};keys.a=true;
    assert(menu.update(keys).action==nlauncher::Action::Prepare);
    menu.report("Teste","Progresso",true,.5f);assert(menu.update(keys).action==nlauncher::Action::None);
    keys={};keys.b=true;assert(menu.update(keys).action==nlauncher::Action::None);assert(menu.wants_cancel());
    menu.finish_job("OK","Concluído");menu.update(keys);keys={};keys.minus=true;menu.update(keys);keys={};keys.y=true;menu.update(keys);
    auto settings=Flow::ReadSettings(Flow::kMenuCfg);assert(settings["games"]==Flow::kDefaultGames);
    nlauncher::Menu again;again.start();keys={};keys.a=true;assert(again.update(keys).action==nlauncher::Action::Launch);
    std::cout<<"PASS: six synthetic titles, split metadata, accents, actions, busy cancellation and folder persistence\n";
}
