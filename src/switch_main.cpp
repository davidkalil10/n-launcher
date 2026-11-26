#include <switch.h>
#include "nx2link/input.hpp"
#include "nx2link/program.hpp"
#include "nx2link/loader_probe.hpp"
#include "nx2link/version.hpp"
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <exception>
#include <string>
#include <vector>

int main() {
    consoleInit(nullptr);
    PadState pad;padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);
    std::filesystem::path directory="sdmc:/switch/n-launcher/input";
    std::vector<std::filesystem::path> entries;
    std::string status="Inspecao somente leitura. Nao executa jogos nesta etapa.";
    size_t selected=0;bool refresh=true;
    while(appletMainLoop()) {
        padUpdate(&pad);auto keys=padGetButtonsDown(&pad);if(keys&HidNpadButton_Plus)break;
        if(keys&HidNpadButton_X) {
            std::printf("\nTestando carregador da sessao atual...\n");consoleUpdate(nullptr);
            try {status=nx2link::run_loader_probe();}catch(const std::exception& e){status=e.what();}
        }
        if(refresh) {
            entries.clear();selected=0;
            try {
                for(const auto& e:std::filesystem::directory_iterator(directory))entries.push_back(e.path());
                std::sort(entries.begin(),entries.end());
            } catch(const std::exception& e) {status=e.what();}
            refresh=false;
        }
        if(keys&HidNpadButton_Down && !entries.empty())selected=(selected+1)%entries.size();
        if(keys&HidNpadButton_Up && !entries.empty())selected=(selected+entries.size()-1)%entries.size();
        if(keys&HidNpadButton_B && directory!=directory.root_path()){directory=directory.parent_path();refresh=true;}

        if(!entries.empty() && (keys&HidNpadButton_Y)) {
            std::error_code ec;std::filesystem::create_directories("sdmc:/switch/n-launcher/logs",ec);
            FILE* report_file=std::fopen("sdmc:/switch/n-launcher/logs/program-prepare.log","wb");
            try {
                std::printf("\nPreparando modulos... aguarde.\n");consoleUpdate(nullptr);
                nx2link::Input input(entries[selected]);
                unsigned count=nx2link::prepare_program(input,[&](const std::string& line){
                    if(report_file){std::fprintf(report_file,"%s\n",line.c_str());std::fflush(report_file);}
                    if(line.rfind("Preparing ",0)==0){std::printf("%s\n",line.c_str());consoleUpdate(nullptr);}
                });
                status="OK: programa preparado ("+std::to_string(count)+" modulos).\nHashes conferidos; nenhum jogo executado.\nLog: /switch/n-launcher/logs/program-prepare.log";
            } catch(const std::exception& e) {
                status=std::string("ERRO ao preparar: ")+e.what();
                if(report_file)std::fprintf(report_file,"FAIL: %s\n",e.what());
            }
            if(report_file)std::fclose(report_file);
        }
        if(!entries.empty() && keys&HidNpadButton_A) {
            auto path=entries[selected];
            auto ext=path.extension().u8string();
            std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
            if(std::filesystem::is_directory(path) && ext!=".nup") {directory=path;refresh=true;}
            else try {
                nx2link::Input in(path);auto info=nx2link::inspect(in);
                char text[256];std::snprintf(text,sizeof(text),"OK: %zu partes, %zu pacotes, %llu arquivos. TOC SHA-256 valido.\nNao verifica payloads nem executa o jogo.",in.parts().size(),info.packages.size(),(unsigned long long)info.files);
                status=text;
            } catch(const std::exception& e) {status=std::string("ERRO: ")+e.what();}
        }
        consoleClear();std::printf("N Launcher " NX2LINK_VERSION " - Loader Probe\n\n%s\n\n",directory.string().c_str());
        const size_t first=(selected/15)*15;
        for(size_t i=first;i<std::min(first+15,entries.size());++i)
            std::printf("%c %s\n",i==selected?'>':' ',entries[i].filename().string().c_str());
        std::printf("\nA: inspecionar | B: voltar | X: carregador | Y: preparar programa | +: sair\n\n%s\n",status.c_str());
        consoleUpdate(nullptr);
    }
    consoleExit(nullptr);return 0;
}
