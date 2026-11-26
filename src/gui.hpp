// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "library.h"
#include "ui_views.h"
#include <map>

namespace nlauncher {
struct Keys {bool a{},b{},x{},y{},plus{},minus{},l{},r{},up{},down{},left{},right{};};
enum class Action {None, Launch, Inspect, Prepare, Probe, Logs, Exit};
struct Request {Action action{};std::string path;};
class Menu {
public:
    void start();
    Request update(Keys keys);
    void draw(SwitchFrontend::Ui::Canvas& canvas);
    void report(std::string title,std::string detail,bool busy=false,float fraction=-1);
    void finish_job(std::string title,std::string detail);
    bool busy() const{return working;}
    bool wants_cancel() const{return cancel;}
private:
    void scan();void folders();void save();
    void text(SwitchFrontend::Ui::Canvas&,int,int,std::string_view,int=20,SwitchFrontend::Ui::u32=0);
    std::string fit(std::string,int,int);
    std::vector<std::string> message_lines();
    std::vector<Flow::Game> games;
    std::map<std::string,SwitchFrontend::Ui::Image> icons;
    SwitchFrontend::Ui::Font font;
    std::string folder{Flow::kDefaultGames},picker{ "/" },title,detail;
    std::vector<std::string> dirs;
    enum class Screen {Library,Picker,Options,Message} screen{Screen::Library};
    int selected{},cursor{},folder_cursor{};
    bool working{},cancel{};float progress{-1};
    std::size_t message_scroll{};unsigned frames{};
};
}
