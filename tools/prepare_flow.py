#!/usr/bin/env python3
"""Verify pinned public sources and generate reviewed adaptations; never edit the snapshot."""
from pathlib import Path
import hashlib, json, shutil

ROOT=Path(__file__).resolve().parents[1]

def prepare():
    pin=json.loads((ROOT/'config/flow.json').read_text(encoding='utf-8'))
    source=ROOT/'third_party/nx2-flow'
    out=ROOT/'build/flow'
    for name,expected in pin['files'].items():
        p=source/name
        if hashlib.sha256(p.read_bytes()).hexdigest()!=expected:
            raise RuntimeError(f'Pinned source changed: {name}')
        target=out/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
    def patch(name,before,after):
        p=out/name;s=p.read_text(encoding='utf-8')
        if s.count(before)!=1:raise RuntimeError(f'Patch no longer applies exactly once: {name}: {before[:70]}')
        p.write_text(s.replace(before,after),encoding='utf-8',newline='\n')
    # Read metadata with exactly the same multipart backend as the validated probe.
    n='menu/source/nup.cpp'
    patch(n,'#include "nup.h"','#include "nup.h"\n#include "nx2link/input.hpp"\n#include <exception>')
    s=(out/n).read_text(encoding='utf-8');a=s.index('bool ReadAt(');b=s.index('// The facts',a)
    s=s[:a]+'''bool ReadAt(const nx2link::Input* f, u64 at, void* to, std::size_t size) {
    try { f->read(at,to,size); return true; } catch (...) { return false; }
}

'''+s[b:]
    a=s.index('    std::FILE* f = std::fopen');b=s.index('    u8 head[0x80];',a)
    s=s[:a]+'''    try {
    const nx2link::Input input{std::filesystem::u8path(path)};
    nx2link::inspect(input); // SHA-256 + bounds, including metadata bounds.
    const auto* f = &input;

'''+s[b:]
    s=s.replace('    return true;\n}\n\n} // namespace Nup','    return true;\n    } catch (const std::exception& e) { error=e.what(); return false; }\n}\n\n} // namespace Nup')
    (out/n).write_text(s,encoding='utf-8',newline='\n')
    n='menu/source/library.h'
    for before,after in [('"nx2-flow"','"N Launcher"'),('"/nx2-flow"','"/switch/n-launcher"'),('"/nx2-flow/games"','"/switch/n-launcher/input"'),('"/nx2-flow/covers"','"/switch/n-launcher/covers"'),('"/nx2-flow/cache"','"/switch/n-launcher/cache"'),('"/nx2-flow/menu.cfg"','"/switch/n-launcher/menu.cfg"'),('"/switch/nx2-flow/nx2-flow.nro"','"/switch/n-launcher/n-launcher.nro"'),('"/switch/nx2-flow/nx2-game.nro"','"/switch/n-launcher/.runtime/n-launcher-game.nro"'),('std::uint64_t size{};','std::uint64_t size{};\n    std::size_t parts{};')]: patch(n,before,after)
    n='menu/source/library.cpp'
    patch(n,'#include "nup.h"','#include "nup.h"\n#include "nx2link/input.hpp"')
    patch(n,'    ListFolders(games_folder, &files);','    auto folders = ListFolders(games_folder, &files);\n    for (const auto& name: folders) if (EndsWith(name,".nup")) files.push_back(name);')
    patch(n,'        game.size = static_cast<std::uint64_t>(st.st_size);\n        game.mtime = static_cast<std::int64_t>(st.st_mtime);', '''        try {
            nx2link::Input input{std::filesystem::u8path(OnCard(game.path))};
            game.size=input.size();game.parts=input.parts().size();
            std::uint64_t fingerprint=1469598103934665603ull;
            for (const auto& part: input.parts()) {
                struct stat ps{};
                if (stat(part.path.string().c_str(),&ps)) throw std::runtime_error("Parte inacessível");
                for (auto value: {part.size,static_cast<std::uint64_t>(ps.st_mtime)}) {
                    for (unsigned i=0;i<8;++i) {fingerprint^=(value>>(8*i))&255;fingerprint*=1099511628211ull;}
                }
            }
            game.mtime=static_cast<std::int64_t>(fingerprint & 0x7fffffffffffffffull);
        } catch (const std::exception& e) {
            game.error=e.what(); game.title=file; games.push_back(std::move(game)); continue;
        }''')
    # Required by runtime_error on all supported compilers.
    patch(n,'#include <cstring>','#include <cstring>\n#include <stdexcept>')
    patch(n,'            mkdir(OnCard(card_path.substr(0, at)).c_str(), 0777);','            std::error_code ec; std::filesystem::create_directory(std::filesystem::u8path(OnCard(card_path.substr(0, at))),ec);')
    # Native UTF-8 paths on SD, wide paths on Windows. C stat/opendir use the Windows ANSI codepage.
    p=out/n;s=p.read_text(encoding='utf-8')
    a=s.index('    struct stat st;',s.index('bool Exists('));b=s.index('\nvoid MakeFolders',a)
    s=s[:a]+'''    std::error_code ec;return std::filesystem::exists(std::filesystem::u8path(OnCard(card_path)),ec);
}
bool IsFolder(const std::string& card_path) {
    std::error_code ec;return std::filesystem::is_directory(std::filesystem::u8path(OnCard(card_path.empty()?"/":card_path)),ec);
}
'''+s[b:]
    a=s.index('    DIR* dir = opendir');b=s.index('    const auto by_name',a)
    s=s[:a]+'''    std::error_code ec;
    for (std::filesystem::directory_iterator it(std::filesystem::u8path(base),ec),end;!ec && it!=end;it.increment(ec)) {
        const auto name=it->path().filename().u8string();
        if (it->is_directory(ec)) folders.push_back(name);
        else if (files && it->is_regular_file(ec)) files->push_back(name);
    }
'''+s[b:]
    a=s.index('        struct stat st;',s.index('std::vector<Game> ScanGames'));b=s.index('        try {',a)
    s=s[:a]+s[b:]
    a=s.index('                struct stat ps{}');b=s.index('                    for (unsigned',a)
    s=s[:a]+'''                const auto modified=std::filesystem::last_write_time(part.path).time_since_epoch().count();
                for (auto value: {part.size,static_cast<std::uint64_t>(modified)}) {
'''+s[b:]
    p.write_text(s,encoding='utf-8',newline='\n')
    from runtime_patches import patch_runtime
    patch_runtime(out)
    print('Verified nx2-flow sources; generated split-aware menu/runtime in build/flow.')
    return out

if __name__=='__main__':prepare()
