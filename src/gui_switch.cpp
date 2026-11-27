// SPDX-License-Identifier: GPL-2.0-or-later
#include <switch.h>
#include "gui.hpp"
#include "nx2link/update.hpp"
#include "nx2link/http.hpp"
#include "nx2link/layout.hpp"
#include "nx2link/version.hpp"
#include "launch_trace.hpp"
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
    nlauncher::launch_trace("runtime.install.begin");
    namespace fs=std::filesystem;
    fs::create_directories("sdmc:/switch/n-launcher/.runtime");
    nlauncher::launch_trace("runtime.directory.ready");
    const fs::path final="sdmc:/switch/n-launcher/.runtime/n-launcher-game.nro",temp="sdmc:/switch/n-launcher/.runtime/n-launcher-game.nro.tmp",old="sdmc:/switch/n-launcher/.runtime/n-launcher-game.nro.previous";
    auto size=static_cast<size_t>(reinterpret_cast<uintptr_t>(nl_runtime_end)-reinterpret_cast<uintptr_t>(nl_runtime_begin));
    nlauncher::launch_trace("runtime.embedded.ready",std::to_string(size));
    {std::error_code ec;if(fs::file_size(final,ec)==size&&!ec){std::ifstream in(final,std::ios::binary);std::vector<uint8_t> existing(size);in.read(reinterpret_cast<char*>(existing.data()),std::streamsize(size));if(in&&nx2link::digest256(existing.data(),existing.size())==nx2link::digest256(nl_runtime_begin,size))return;}}
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(nl_runtime_begin),std::streamsize(size));out.close();if(!out)throw std::runtime_error("Não foi possível gravar o carregador no SD");}
    nlauncher::launch_trace("runtime.file.written");
    bool previous=fs::exists(final);
    if(previous){fs::remove(old);fs::rename(final,old);}
    try{fs::rename(temp,final);}catch(...){if(previous)fs::rename(old,final);throw;}
    if(previous)fs::remove(old);
    nlauncher::launch_trace("runtime.install.ready");
}
struct Job {
    Thread thread{};std::mutex lock;std::atomic<bool> done{},cancel{};
    nlauncher::Request request;std::string message,result;bool success{},automatic{};std::atomic<float> fraction{-1};
    nx2link::UpdateManifest manifest;std::filesystem::path executable;bool newer{};
    void status(std::string s){std::lock_guard<std::mutex> guard(lock);message=std::move(s);}
};
void work(void* arg){
    auto& job=*static_cast<Job*>(arg);
    FILE* log=nullptr;
    try{
        const bool launching=job.request.action==nlauncher::Action::Launch;
        if(launching)nlauncher::launch_trace("worker.started");
        std::filesystem::create_directories("sdmc:/switch/n-launcher/logs");
        const bool updating=job.request.action==nlauncher::Action::CheckUpdate||job.request.action==nlauncher::Action::InstallUpdate;
        log=std::fopen(updating?"sdmc:/switch/n-launcher/logs/update-operation.log":"sdmc:/switch/n-launcher/logs/menu-operation.log","wb");
        auto fetch=[&](const std::string& url,const nx2link::UpdateSink& sink,uint64_t limit){nx2link::fetch_update_https(url,[&](const uint8_t* bytes,size_t n){if(job.cancel.load())throw std::runtime_error("Atualização cancelada; aplicativo atual preservado");sink(bytes,n);},limit);};
        if(job.request.action==nlauncher::Action::CheckUpdate){
            job.status("Consultando a versão pública no GitHub…");job.manifest=nx2link::check_update(fetch);job.newer=nx2link::version_newer(job.manifest.version,NX2LINK_VERSION);job.result="Você já usa a versão mais recente: " NX2LINK_VERSION;
        }else if(job.request.action==nlauncher::Action::InstallUpdate){
            nx2link::install_update(job.manifest,NX2LINK_VERSION,job.executable,fetch,[&](const std::string& step,double fraction){if(job.cancel.load())throw std::runtime_error("Atualização cancelada; aplicativo atual preservado");job.status(step);job.fraction.store(float(fraction));});job.result="Atualização verificada e instalada. Reabra o aplicativo.";
        }else if(job.request.action==nlauncher::Action::Probe){job.status("Testando o carregador da sessão ativa…");job.result=nx2link::run_loader_probe();}
        else{
            nx2link::Input input{std::filesystem::u8path(Flow::OnCard(job.request.path))};
            if(job.request.action==nlauncher::Action::Prepare||job.request.action==nlauncher::Action::Launch){
                if(launching)nlauncher::launch_trace("program.begin");
                auto n=nx2link::prepare_program(input,[&](const std::string& line){
                    if(log){std::fprintf(log,"%s\n",line.c_str());std::fflush(log);}
                    if(launching)nlauncher::launch_trace("program.step",line);
                    if(job.cancel.load())throw std::runtime_error("Operação cancelada. O NUP original foi preservado.");
                    job.status(line);
                });
                if(launching)nlauncher::launch_trace("program.ready");
                if(launching)nlauncher::launch_trace("cache.begin");
                auto cached=nx2link::prepare_cache(input,"sdmc:/switch/n-launcher/content",[&](const std::string& message,double value){
                    if(job.cancel.load())throw std::runtime_error("Preparação cancelada. O NUP original foi preservado.");
                    job.status(message);job.fraction.store(float(value));
                });
                if(launching)nlauncher::launch_trace("cache.ready",std::to_string(cached));
                if(job.cancel.load())throw std::runtime_error("Preparação cancelada antes de iniciar o jogo. Cache verificado preservado.");
                if(job.request.action==nlauncher::Action::Launch)install_runtime();
                job.result="Programa preparado: "+std::to_string(n)+" módulos.\nCache: "+std::to_string(cached)+" arquivos grandes verificados.";
            }else{auto info=nx2link::inspect(input);job.result="Índice SHA-256 válido.\n"+std::to_string(input.parts().size())+" partes · "+std::to_string(info.packages.size())+" pacotes · "+std::to_string(info.files)+" arquivos.\nO conteúdo de cada arquivo ainda não foi verificado.";}
        }
        job.success=true;
    }catch(const std::exception& e){job.result=e.what();if(job.request.action==nlauncher::Action::Launch)nlauncher::launch_trace("worker.error",job.result);}
    if(log){std::fprintf(log,"%s\n",job.result.c_str());std::fclose(log);}
    if(job.request.action==nlauncher::Action::Launch)nlauncher::launch_trace("worker.finished",job.success?"success":"failed");
    job.done.store(true);
}
void present(Framebuffer& fb,SwitchFrontend::Ui::Canvas& c){u32 pitch;auto* out=static_cast<u8*>(framebufferBegin(&fb,&pitch));for(int y=0;y<c.Height();++y)std::memcpy(out+y*pitch,c.Data()+y*c.Width(),c.Width()*4);framebufferEnd(&fb);}
}
int main(int argc,char** argv){
    using namespace nlauncher;
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);
    Framebuffer fb{};if(R_FAILED(framebufferCreate(&fb,nwindowGetDefault(),1280,720,PIXEL_FORMAT_RGBA_8888,2)))return 1;framebufferMakeLinear(&fb);
    Flow::SetCardRoot("sdmc:");auto canvas=std::make_unique<SwitchFrontend::Ui::Canvas>();Menu menu;std::unique_ptr<Job> job;
    std::string menu_path=argc>0&&argv&&argv[0]?argv[0]:Flow::OnCard(Flow::kMenuNro);
    bool startup_error=false;
    try{auto migration=nx2link::migrate_layout("sdmc:/",menu_path);menu_path=migration.executable;nx2link::confirm_update(std::filesystem::u8path(menu_path),NX2LINK_VERSION);menu.start();}
    catch(const std::exception& e){startup_error=true;menu.report("Dados preservados",std::string(e.what())+". Feche o aplicativo para organizar as pastas.");}
    nx2link::UpdateManifest pending_update;
    bool leave=false;
    while(appletMainLoop()){
        padUpdate(&pad);auto bits=padGetButtonsDown(&pad);Keys k;
        k.a=bits&HidNpadButton_A;k.b=bits&HidNpadButton_B;k.x=bits&HidNpadButton_X;k.y=bits&HidNpadButton_Y;k.plus=bits&HidNpadButton_Plus;k.minus=bits&HidNpadButton_Minus;
        k.r=bits&HidNpadButton_R;k.l=bits&HidNpadButton_L;
        if(startup_error){if(k.b||k.plus)break;menu.draw(*canvas);present(fb,*canvas);continue;}
        k.up=bits&HidNpadButton_AnyUp;k.down=bits&HidNpadButton_AnyDown;k.left=bits&HidNpadButton_AnyLeft;k.right=bits&HidNpadButton_AnyRight;
        bool completed_job=false;
        if(job){
            if(k.b)job->cancel.store(true);
            if(job->done.load()){
                threadWaitForExit(&job->thread);threadClose(&job->thread);
                if(job->success&&job->cancel.load()&&job->request.action==Action::Launch){job->success=false;job->result="Preparação cancelada antes de iniciar o jogo. Cache verificado preservado.";}
                if(job->success&&job->request.action==Action::Launch){
                    const auto loader=Flow::OnCard(Flow::kGameNro);
                    const std::string args=nx2link::launch_arguments(loader,Flow::OnCard(job->request.path),menu_path,job->request.memory.loader_mb);
                    launch_trace("handoff.begin",loader);
                    const auto next_rc=envSetNextLoad(loader.c_str(),args.c_str());
                    launch_trace("handoff.result",std::to_string(next_rc));
                    if(R_SUCCEEDED(next_rc))leave=true;
                    else{job->success=false;job->result="O hbmenu não aceitou iniciar o carregador. Consulte os registros.";}
                }
                if(job->success&&job->request.action==Action::CheckUpdate&&job->cancel.load()){job->success=false;job->result="Verificação cancelada.";}
                if(job->success&&job->request.action==Action::CheckUpdate&&job->newer){pending_update=job->manifest;menu.offer_update(pending_update.version,pending_update.size);}
                else if(job->success&&job->request.action==Action::InstallUpdate)menu.offer_restart();
                else if(job->automatic)menu.finish_auto_check(!job->success,job->cancel.load());
                else menu.finish_job(job->success?"Operação concluída":"Operação interrompida",job->result+"\nRegistro: /switch/n-launcher/logs/");
                job.reset();completed_job=true;
            }else{std::lock_guard<std::mutex> guard(job->lock);menu.report(job->request.action==Action::CheckUpdate||job->request.action==Action::InstallUpdate?"Atualizações":"Preparando e verificando",job->message,true,job->fraction.load());}
        }
        if(leave)break;
        // A newly displayed update prompt requires a fresh confirmation press.
        if(completed_job)k={};
        auto req=menu.update(k);bool automatic_request=menu.begin_startup_check(req.action,bool(job));
        if(automatic_request)req.action=Action::CheckUpdate;
        if(req.action==Action::Exit)break;
        if(req.action==Action::Restart){
            const std::string args="\""+menu_path+"\"";
            if(R_SUCCEEDED(envSetNextLoad(menu_path.c_str(),args.c_str()))){leave=true;break;}
            menu.offer_restart();
        }else if(req.action==Action::Logs){
            std::ifstream in("sdmc:/switch/n-launcher/logs/menu-operation.log",std::ios::binary);std::string tail;
            if(in){in.seekg(0,std::ios::end);std::streamoff bytes=in.tellg();in.seekg(bytes>8192?bytes-8192:0);tail.assign(std::istreambuf_iterator<char>(in),{});}
            menu.report("Última operação",tail.empty()?"Nenhum registro disponível ainda.":tail);
        }else if(req.action==Action::Launch&&(!envHasNextLoad()||smGetServiceOverride(smEncodeName("ldr:ro"))==INVALID_HANDLE)){
            menu.report("Sessão necessária","Abra pelo hbmenu com kaiHEN ativo. Esta homebrew utiliza a sessão existente; ela não instala ou reativa o kaiHEN.");
        }else if(req.action==Action::Launch&&!begin_launch_trace(Flow::OnCard(req.path))){
            menu.report("Não foi possível iniciar","Não foi possível preservar o registro de abertura no SD. Confira o espaço livre e os arquivos de logs.");
        }else if(req.action!=Action::None){
            job=std::make_unique<Job>();job->request=req;job->automatic=automatic_request;job->executable=std::filesystem::u8path(menu_path);if(req.action==Action::InstallUpdate)job->manifest=pending_update;
            auto rc=threadCreate(&job->thread,work,job.get(),nullptr,0x40000,0x2c,-2);
            if(R_SUCCEEDED(rc)){rc=threadStart(&job->thread);if(R_FAILED(rc))threadClose(&job->thread);}
            if(R_FAILED(rc)){job.reset();if(automatic_request)menu.finish_auto_check(true);else menu.report("Não foi possível iniciar","A operação não começou. Seus arquivos não foram alterados.");}
            else menu.report(automatic_request?"Verificando atualizações ao abrir":"Iniciando verificação","Aguarde…",true);
        }
        menu.draw(*canvas);present(fb,*canvas);
    }
    // Do not release shared buffers while a native worker still owns them.
    if(job){job->cancel.store(true);threadWaitForExit(&job->thread);threadClose(&job->thread);}
    if(leave)launch_trace("frontend.exit.for.next_app");
    framebufferClose(&fb);return 0;
}
