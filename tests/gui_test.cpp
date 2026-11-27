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
    nlauncher::Menu startup;startup.start();assert(startup.startup_updates_enabled());
    assert(startup.begin_startup_check(nlauncher::Action::None,false));assert(!startup.begin_startup_check(nlauncher::Action::None,false));
    nlauncher::Menu delayed;delayed.start();assert(!delayed.begin_startup_check(nlauncher::Action::None,true));assert(delayed.begin_startup_check(nlauncher::Action::None,false));
    nlauncher::Menu manual;manual.start();assert(!manual.begin_startup_check(nlauncher::Action::CheckUpdate,false));assert(!manual.begin_startup_check(nlauncher::Action::None,false));
    auto startupCfg=Flow::ReadSettings(Flow::kMenuCfg);startupCfg["other_setting"]="keep";Flow::WriteSettings(Flow::kMenuCfg,startupCfg);
    keys={};keys.l=true;startup.update(keys);assert(!startup.in_library());keys={};keys.a=true;startup.update(keys);assert(!startup.startup_updates_enabled());keys={};keys.b=true;startup.update(keys);
    nlauncher::Menu disabled;disabled.start();assert(!disabled.startup_updates_enabled());assert(!disabled.begin_startup_check(nlauncher::Action::None,false));
    keys={};keys.minus=true;disabled.update(keys);keys={};keys.y=true;disabled.update(keys);auto persisted=Flow::ReadSettings(Flow::kMenuCfg);assert(persisted["check_updates"]=="0"&&persisted["other_setting"]=="keep");
    keys={};keys.r=true;assert(disabled.update(keys).action==nlauncher::Action::CheckUpdate);disabled.report("Auto","",true);disabled.finish_auto_check(true);assert(disabled.in_library()&&!disabled.busy());
    keys={};keys.l=true;disabled.update(keys);keys={};keys.a=true;disabled.update(keys);keys={};keys.b=true;disabled.update(keys);nlauncher::Menu enabled;enabled.start();assert(enabled.startup_updates_enabled());
    assert(!enabled.is_english());
    keys={};keys.l=true;enabled.update(keys);keys={};keys.down=true;enabled.update(keys);keys={};keys.a=true;enabled.update(keys);assert(enabled.is_english());keys={};keys.b=true;enabled.update(keys);
    auto langCfg=Flow::ReadSettings(Flow::kMenuCfg);assert(langCfg["language"]=="en"&&langCfg["check_updates"]=="1"&&langCfg["other_setting"]=="keep");
    nlauncher::Menu englishMenu;englishMenu.start();assert(englishMenu.is_english());
    keys={};keys.l=true;englishMenu.update(keys);keys={};keys.down=true;englishMenu.update(keys);keys={};keys.right=true;englishMenu.update(keys);assert(!englishMenu.is_english());keys={};keys.b=true;englishMenu.update(keys);
    assert(Flow::ReadSettings(Flow::kMenuCfg)["language"]=="pt");
    enabled.report("Auto","",true);enabled.finish_auto_check(false);assert(enabled.in_library()&&!enabled.busy());
    std::cout<<"PASS: six synthetic titles, split metadata, accents, actions, busy cancellation, profile persistence, startup-check policy, language toggle, preferences and update confirmation\n";
}
