// SPDX-License-Identifier: GPL-2.0-or-later
#include <switch.h>
#include "gui.hpp"
#include "nx2link/input.hpp"
#include "nx2link/program.hpp"
#include "nx2link/loader_probe.hpp"
#include "nx2link/cache.hpp"
#include "nx2link/crypto.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <cstring>
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <vector>
extern "C" const unsigned char nl_runtime_begin[],nl_runtime_end[];
namespace {
void install_runtime(){
    namespace fs=std::filesystem;
    fs::create_directories("sdmc:/switch/nx2-link/.runtime");
    const fs::path final="sdmc:/switch/nx2-link/.runtime/n-launcher-game.nro",temp="sdmc:/switch/nx2-link/.runtime/n-launcher-game.nro.tmp",old="sdmc:/switch/nx2-link/.runtime/n-launcher-game.nro.previous";
    auto size=static_cast<size_t>(nl_runtime_end-nl_runtime_begin);
    {std::error_code ec;if(fs::file_size(final,ec)==size&&!ec){std::ifstream in(final,std::ios::binary);std::vector<uint8_t> existing(size);in.read(reinterpret_cast<char*>(existing.data()),std::streamsize(size));if(in&&nx2link::digest256(existing.data(),existing.size())==nx2link::digest256(nl_runtime_begin,size))return;}}
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(nl_runtime_begin),std::streamsize(size));out.close();if(!out)throw std::runtime_error("Não foi possível gravar o carregador no SD");}
    bool previous=fs::exists(final);
    if(previous){fs::remove(old);fs::rename(final,old);}
    try{fs::rename(temp,final);}catch(...){if(previous)fs::rename(old,final);throw;}
    if(previous)fs::remove(old);
}
struct Job {
    Thread thread{};std::mutex lock;std::atomic<bool> done{},cancel{};
    nlauncher::Request request;std::string message,result;bool success{};std::atomic<float> fraction{-1};
    void status(std::string s){std::lock_guard<std::mutex> guard(lock);message=std::move(s);}
};
void work(void* arg){
    auto& job=*static_cast<Job*>(arg);
    FILE* log=nullptr;
    try{
        std::filesystem::create_directories("sdmc:/switch/nx2-link/logs");
        log=std::fopen("sdmc:/switch/nx2-link/logs/menu-operation.log","wb");
        if(job.request.action==nlauncher::Action::Probe){job.status("Testando o carregador da sessão ativa…");job.result=nx2link::run_loader_probe();}
        else{
            nx2link::Input input{std::filesystem::u8path(Flow::OnCard(job.request.path))};
            if(job.request.action==nlauncher::Action::Prepare||job.request.action==nlauncher::Action::Launch){
                auto n=nx2link::prepare_program(input,[&](const std::string& line){
                    if(log){std::fprintf(log,"%s\n",line.c_str());std::fflush(log);}
                    if(job.cancel.load())throw std::runtime_error("Operação cancelada. O NUP original foi preservado.");
                    job.status(line);
                });
                auto cached=nx2link::prepare_cache(input,"sdmc:/switch/nx2-link/content",[&](const std::string& message,double value){
                    if(job.cancel.load())throw std::runtime_error("Preparação cancelada. O NUP original foi preservado.");
                    job.status(message);job.fraction.store(float(value));
                });
                if(job.request.action==nlauncher::Action::Launch)install_runtime();
                job.result="Programa preparado: "+std::to_string(n)+" módulos.\nCache: "+std::to_string(cached)+" arquivos grandes verificados.";
            }else{auto info=nx2link::inspect(input);job.result="Índice SHA-256 válido.\n"+std::to_string(input.parts().size())+" partes · "+std::to_string(info.packages.size())+" pacotes · "+std::to_string(info.files)+" arquivos.\nO conteúdo de cada arquivo ainda não foi verificado.";}
        }
        job.success=true;
    }catch(const std::exception& e){job.result=e.what();}
    if(log){std::fprintf(log,"%s\n",job.result.c_str());std::fclose(log);}
    job.done.store(true);
}
void present(Framebuffer& fb,SwitchFrontend::Ui::Canvas& c){u32 pitch;auto* out=static_cast<u8*>(framebufferBegin(&fb,&pitch));for(int y=0;y<c.Height();++y)std::memcpy(out+y*pitch,c.Data()+y*c.Width(),c.Width()*4);framebufferEnd(&fb);}
}
int main(int argc,char** argv){
    using namespace nlauncher;
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);
    Framebuffer fb{};if(R_FAILED(framebufferCreate(&fb,nwindowGetDefault(),1280,720,PIXEL_FORMAT_RGBA_8888,2)))return 1;framebufferMakeLinear(&fb);
    Flow::SetCardRoot("sdmc:");auto canvas=std::make_unique<SwitchFrontend::Ui::Canvas>();Menu menu;menu.start();std::unique_ptr<Job> job;
    std::string menu_path=argc>0&&argv&&argv[0]?argv[0]:Flow::OnCard(Flow::kMenuNro);
    bool leave=false;
    while(appletMainLoop()){
        padUpdate(&pad);auto bits=padGetButtonsDown(&pad);Keys k;
        k.a=bits&HidNpadButton_A;k.b=bits&HidNpadButton_B;k.x=bits&HidNpadButton_X;k.y=bits&HidNpadButton_Y;k.plus=bits&HidNpadButton_Plus;k.minus=bits&HidNpadButton_Minus;
        k.up=bits&HidNpadButton_AnyUp;k.down=bits&HidNpadButton_AnyDown;k.left=bits&HidNpadButton_AnyLeft;k.right=bits&HidNpadButton_AnyRight;
        if(job){
            if(k.b)job->cancel.store(true);
            if(job->done.load()){
                threadWaitForExit(&job->thread);threadClose(&job->thread);
                if(job->success&&job->request.action==Action::Launch){
                    const auto loader=Flow::OnCard(Flow::kGameNro);
                    const std::string args="\""+loader+"\" \""+Flow::OnCard(job->request.path)+"\" \"menu="+menu_path+"\"";
                    if(R_SUCCEEDED(envSetNextLoad(loader.c_str(),args.c_str())))leave=true;
                    else{job->success=false;job->result="O hbmenu não aceitou iniciar o carregador. Consulte os registros.";}
                }
                menu.finish_job(job->success?"Verificação concluída":"Operação interrompida",job->result+"\nRegistro: /switch/nx2-link/logs/");job.reset();
            }else{std::lock_guard<std::mutex> guard(job->lock);menu.report("Preparando e verificando",job->message,true,job->fraction.load());}
        }
        if(leave)break;
        auto req=menu.update(k);
        if(req.action==Action::Exit)break;
        if(req.action==Action::Logs){
            std::ifstream in("sdmc:/switch/nx2-link/logs/menu-operation.log",std::ios::binary);std::string tail;
            if(in){in.seekg(0,std::ios::end);std::streamoff bytes=in.tellg();in.seekg(bytes>8192?bytes-8192:0);tail.assign(std::istreambuf_iterator<char>(in),{});}
            menu.report("Última operação",tail.empty()?"Nenhum registro disponível ainda.":tail);
        }else if(req.action==Action::Launch&&(!envHasNextLoad()||smGetServiceOverride(smEncodeName("ldr:ro"))==INVALID_HANDLE)){
            menu.report("Sessão necessária","Abra pelo hbmenu com kaiHEN ativo. Esta homebrew utiliza a sessão existente; ela não instala ou reativa o kaiHEN.");
        }else if(req.action!=Action::None){
            job=std::make_unique<Job>();job->request=req;
            auto rc=threadCreate(&job->thread,work,job.get(),nullptr,0x40000,0x2c,-2);
            if(R_SUCCEEDED(rc)){rc=threadStart(&job->thread);if(R_FAILED(rc))threadClose(&job->thread);}
            if(R_FAILED(rc)){job.reset();menu.report("Não foi possível iniciar","A operação não começou. Seus arquivos não foram alterados.");}
            else menu.report("Iniciando verificação","Aguarde…",true);
        }
        menu.draw(*canvas);present(fb,*canvas);
    }
    // Do not release shared buffers while a native worker still owns them.
    if(job){job->cancel.store(true);threadWaitForExit(&job->thread);threadClose(&job->thread);}
    framebufferClose(&fb);return 0;
}
