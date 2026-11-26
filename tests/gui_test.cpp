// SPDX-License-Identifier: GPL-2.0-or-later
#include "gui.hpp"
#include <cassert>
#include <iostream>
int main(int argc,char** argv){
    assert(argc==2);Flow::SetCardRoot(argv[1]);
    auto games=Flow::ScanGames(Flow::kDefaultGames);assert(games.size()==6);unsigned split=0;bool accented=false;
    for(const auto& game:games){assert(game.error.empty());assert(!game.id.empty());split+=game.parts==2;accented|=game.title=="Café Nebula";}
    assert(split==2&&accented);
    Flow::WriteSettings(games[0].Folder()+"/game.cfg",{{"save","slot 2"},{"mods","night"}});
    nlauncher::Menu menu;menu.start();nlauncher::Keys keys;keys.y=true;menu.update(keys);keys={};keys.a=true;menu.update(keys);
    keys={};keys.right=true;menu.update(keys);keys={};keys.down=true;menu.update(keys);keys={};keys.right=true;menu.update(keys);keys={};keys.a=true;menu.update(keys);
    auto cfg=Flow::ReadSettings(games[0].Folder()+"/game.cfg");assert(cfg["memory"]=="2048"&&cfg["room"]=="128"&&cfg["save"]=="slot 2"&&cfg["mods"]=="night");
    keys={};keys.b=true;menu.update(keys);keys={};keys.a=true;auto launch=menu.update(keys);assert(launch.action==nlauncher::Action::Launch&&launch.memory.game_mb==2048&&launch.memory.loader_mb==128);
    keys={};keys.y=true;menu.update(keys);keys={};keys.a=true;menu.update(keys);keys={};keys.x=true;menu.update(keys);keys={};keys.a=true;menu.update(keys);keys={};keys.b=true;menu.update(keys);
    keys={};keys.y=true;menu.update(keys);keys={};keys.down=true;menu.update(keys);keys={};keys.a=true;assert(menu.update(keys).action==nlauncher::Action::Prepare);
    menu.report("Teste","Progresso",true,.5f);assert(menu.update(keys).action==nlauncher::Action::None);
    keys={};keys.b=true;assert(menu.update(keys).action==nlauncher::Action::None);assert(menu.wants_cancel());
    menu.finish_job("OK","Concluído");menu.update(keys);keys={};keys.minus=true;menu.update(keys);keys={};keys.y=true;menu.update(keys);
    auto settings=Flow::ReadSettings(Flow::kMenuCfg);assert(settings["games"]==Flow::kDefaultGames);
    nlauncher::Menu again;again.start();keys={};keys.a=true;assert(again.update(keys).action==nlauncher::Action::Launch);
    keys={};keys.r=true;assert(again.update(keys).action==nlauncher::Action::CheckUpdate);again.offer_update("0.0.7",20000000);keys={};keys.a=true;assert(again.update(keys).action==nlauncher::Action::InstallUpdate);again.offer_restart();keys={};keys.b=true;assert(again.update(keys).action==nlauncher::Action::Exit);
    std::cout<<"PASS: six synthetic titles, split metadata, accents, actions, busy cancellation, profile persistence and update confirmation\n";
}
