// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "library.h"
#include "nx2link/profile.hpp"
#include "ui_views.h"
#include <map>

namespace nlauncher {
struct Keys {bool a{},b{},x{},y{},plus{},minus{},l{},r{},up{},down{},left{},right{};};
enum class Action {None, Launch, Inspect, Prepare, Probe, Logs, CheckUpdate, InstallUpdate, Restart, Exit};
struct Request {Action action{};std::string path;nx2link::MemoryProfile memory{};};
class Menu {
public:
    void start();
    Request update(Keys keys);
    void draw(SwitchFrontend::Ui::Canvas& canvas);
    void report(std::string title,std::string detail,bool busy=false,float fraction=-1);
    void offer_update(const std::string& version,uint64_t size);
    void offer_restart();
    void finish_auto_check(bool failed,bool canceled=false);
    bool startup_updates_enabled()const{return automatic_updates;}
    bool begin_startup_check(Action requested,bool operation_active);
    bool in_library()const{return screen==Screen::Library;}
    void finish_job(std::string title,std::string detail);
    bool busy() const{return working;}
    bool wants_cancel() const{return cancel;}
private:
    Request request(Action);
    void open_memory();
    void scan();void folders();void save();
    void text(SwitchFrontend::Ui::Canvas&,int,int,std::string_view,int=20,SwitchFrontend::Ui::u32=0);
    std::string fit(std::string,int,int);
    std::vector<std::string> message_lines();
    std::vector<Flow::Game> games;
    std::map<std::string,SwitchFrontend::Ui::Image> icons;
    SwitchFrontend::Ui::Font font;
    std::string folder{Flow::kDefaultGames},picker{ "/" },title,detail;
    std::vector<std::string> dirs;
    enum class Screen {Library,Picker,Options,Message,Memory,Update,Restart,Settings} screen{Screen::Library};
    int selected{},cursor{},folder_cursor{},memory_cursor{};
    nx2link::MemoryProfile memory{};std::string update_version;uint64_t update_size{};
    bool working{},cancel{},automatic_updates{true},startup_check_pending{true};float progress{-1};
    std::string update_notice;
    std::size_t message_scroll{};unsigned frames{};
};
}
