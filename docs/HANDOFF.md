# Continuidade — N Launcher

## Objetivo e estado

Preparar e iniciar NUPs diretamente no Switch 2, incluindo pastas `.nup/00,01,…`, sem compilar um port por jogo no PC e sem reativar kaiHEN para atualizar a NRO. Nome público: **N Launcher**; repositório e namespace C++ nx2-link-enhanced/nx2link são mantidos; a pasta do SD passa a n-launcher em 0.0.6.

**Pokopia splitado foi validado no console em 0.0.5: imagem, som, controles, salvar e carregar funcionam.** O frontend newlib resolveu o fechamento observado; isso não demonstra um defeito geral do SDK. A versão 0.0.6 preserva esse caminho e acrescenta migração, perfis de memória e atualizador. Os recursos novos precisam de validação própria no console, especialmente TLS e troca do aplicativo.

## Organização das fontes

- `src/input.cpp`: leitor comum completo/split e verificação da TOC; offsets cumulativos de 64 bits.
- `src/archive.hpp`: travessia verificada compartilhada, seleção de módulos e overlay de RomFS base/updates.
- `src/program.cpp`: preparação NSO, LZ4, hashes e inventário SVC. Preserva SVC 0x36. Preflight ainda rejeita ZBIC.
- `src/cache.cpp`: descompressão streaming dos arquivos grandes de RomFS, SHA incremental, staging, espaço e publicação.
- `src/gui.cpp`: interface gráfica independente do console; `gui_switch.cpp`: framebuffer, controle, thread de preparação e handoff.
- `src/native_input.cpp`: setup com Input; leituras no contexto do jogo usam somente estruturas POD, locks atômicos e FS nativo. Não usar STL, alocação ou C stdio no caminho das threads do jogo.
- `third_party/nx2-flow`: cópia original fixada; `config/flow.json`: commit, versões, licenças e hashes.
- `tools/prepare_flow.py` e `runtime_patches.py`: verificam snapshot e geram adaptações em `build/flow/`. Não editar os arquivos gerados como fonte da solução.
- `third_party/nx2-relink/runtime`: referência original 0.1.12 com correção de áudio; não é o runtime linkado. `audio2.c` corresponde ao do loader fixado do nx2-flow.

## Caminho de execução

Menu -> thread própria: preparar/verificar NSOs -> preparar cache RomFS grande -> materializar NRO interno -> `envSetNextLoad` com caminho do NUP e retorno ao menu -> runtime genérico lê o NUP/split, prepara imagens e RomFS virtual, resolve módulos/imports/TLS/thunks e usa `ldr:ro` -> jogo.

O frontend inclui o NRO genérico como recurso. O usuário copia somente o NRO principal. O carregador é materializado em `/switch/n-launcher/.runtime/`, com arquivo temporário e preservação da versão anterior durante a troca. Isso não instala ou atualiza kaiHEN. A chamada do carregador usa a sessão herdada do hbmenu.

O loader fixado faz merge dos módulos que não precisam ficar whole para respeitar o limite de sete imagens adicionais do serviço RO. Implementação de áudio, relocações, SVC, TLS e GPU nativa permanece upstream. O adaptador altera fonte de dados, cache e namespaces, não tenta executar código de jogo de heap RW.

## Cache e memória

O loader upstream descomprime arquivos inteiros para RAM. Pokopia contém um arquivo expandido de cerca de 2,56 GB, acima da área reservável do loader. Nossa adaptação troca arquivos comprimidos maiores que 64 MiB por entradas kind=3, lidas do cache em partes de 1 GiB. Arquivos pequenos continuam no cache RAM upstream. Arquivos sem compressão são lidos diretamente do NUP/split.

O cache é por SHA-256 do conteúdo, com marcador NLC1 e tamanho/partes. Não depende do nome do jogo nem do runtime: são bytes originais descomprimidos, sem relocações. Consulte `docs/CACHE.md`. Não concatenar o jogo nem gerar um RomFS gigante como contorno para o split.

## Testes e build

- `scripts/test-host.ps1`: leitor, preparação e ownership do módulo sintético.
- `scripts/test-launcher.ps1`: cache streaming e o backend nativo real com FS simulado.
- `tools/build_gui.py --preview --compiler <g++>`: biblioteca sintética, metadados split/acentos, ações, cancelamento e persistência; imagens com o mesmo renderer do console.
- `scripts/build-switch2.ps1`: gera prova, runtime e frontend; verifica ambos os pacotes e mapas. Pillow da build está fixado em requirements-build.txt.

Separar testes locais, verificação de binário e validação de console em todos os relatos. Não enviar jogos, chaves, saves, caminhos privados ou logs de SD ao CI/Git. Fixtures próprias apenas. Não modificar Eden, builder Windows, dumps ou saves usados como referência.

## Próximo teste no hardware

Instalar o ZIP 0.0.6 manualmente. É possível renomear nx2-link para n-launcher antes de extrair; o aplicativo também migra dados por renomeação sem copiar os gigabytes. Não sobrescreve conflitos. Abrir a nova versão com kaiHEN existente, conferir biblioteca/cache/saves e jogar com perfil Automático. Depois testar Y → Memória por jogo e R → Atualizações. Não é necessário reativar kaiHEN.

Não acessar um IP antigo do console sem confirmação. Não reiniciar automaticamente. Retorno espontâneo ao hbmenu e outros jogos não foram confirmados pelo teste do Pokopia.

## Limitações abertas

Preflight ZBIC não implementado; PFS0/NCA criptografados não suportados; .nup.00 existe no leitor, mas não no catálogo. Cache publicado confere marcador/tamanhos sem rehash completo; cancelar reinicia o arquivo parcial. Limpeza e revalidação integral de cache ainda sem interface. TLS, atualização efetiva e migração no hardware precisam de teste. Nunca declarar compatibilidade geral a partir de um jogo.

## Investigação de fechamento — diagnóstico 0.0.5

Branch `codex/diagnose-preflight-exit`. A tentativa isolada do Pokopia em 0.0.4 encerrou depois do relatório SHA-256 da imagem main, antes do próximo módulo, do cache e da instalação do loader. Isso delimita a falha ao preflight/GUI; não confirma defeito do NUP nem prova uma causa específica no alocador. A pasta .runtime e os logs de jogo não existiam. Os 11 caches NLC1 estavam completos por marcador e tamanho (6.526.474.702 bytes; nenhum rehash integral pelo FTP).

A build de diagnóstico usa libounce_core/newlib apenas no frontend, alternativa documentada pelo próprio SDK, em lugar de mimalloc. O runtime, os NSOs, o SDK fixado e o formato de cache não foram trocados. `tools/build_gui.py --allocator mimalloc` permite comparação controlada; o padrão diagnóstico é newlib. O mapa confirma core e ausência de mimalloc. Não atribuir a causa ao SDK antes do teste no console.

`src/program.cpp` agora delimita e registra a liberação da imagem e da fonte de cada módulo. `src/launch_trace.cpp` preserva launch.log/launch.previous.log, fecha e sincroniza cada etapa e informa memória do processo, antes/depois dos módulos, do cache, da instalação e do handoff. A saída do processo sem novas linhas deixa o último ponto confirmado. Não executar outro jogo antes de copiar os registros da tentativa relevante.

28 testes de leitura/programa e verificação estrita dos NROs passaram localmente. A correção de memória e a execução ainda dependem de validação. Não pedir nova preparação de gigabytes: o cache publicado mantém os mesmos hashes, marcador e tamanho de parte e deve ser reutilizado. Jogos e saves do SD foram apenas lidos durante o diagnóstico.

## Implementação 0.0.6

- Caminho canônico: /switch/n-launcher/n-launcher.nro; layout.cpp migra a raiz legada por rename e atualiza menu.cfg. Conflitos nas duas raízes interrompem a migração sem sobrescrever dados. NROs antigos podem permanecer na raiz legada; abrir a versão nova.
- profile.cpp valida reservas; gui.cpp grava game.cfg por staging e cópia anterior, preservando campos desconhecidos. Perfis por Title ID, memory para jogo e room para loader; valores zero mantêm Automático. O argumento room numérico permanece sem aspas para o parser inicial do upstream.
- update.cpp: manifesto estrito schema/version/asset/size/sha256, NRO Switch 2/identidade/versão e hash conferidos antes de substituir. Mantém .previous e .update-state até a nova versão confirmar. Uma queda de energia entre renames pode exigir recuperação manual; consultar UPDATES.md.
- http.cpp limita e valida HTTP; https_switch.cpp usa TLS nativo com CA/host/data verificados e apenas hosts públicos de assets do GitHub. Sem token no console. Consulta R, confirmação A, download em streaming. Primeira instalação manual.
- package_release.py produz NRO, ZIP com licenças, update.ini e SHA256SUMS.txt. Actions testa Linux, compila/verifica Windows e gera rascunhos; tag v<versão> da main ou dispatch publish_release publica o feed /releases/latest/download/update.ini. Runtime upstream não é atualizado automaticamente.
- Testes: 38 verificações do núcleo de atualização/perfil/migração; suítes de leitor, NSO, cache e FS nativo; GUI com perfil persistido, cancelamento e confirmação de update. Distinguir estes testes de validação de TLS no hardware.

## Distribuição 0.0.7

Repositório oficial público: https://github.com/davidkalil10/n-launcher. URL central do updater em include/nx2link/update.hpp. Git remote e empacotador usam o novo nome. O teste solicitado é atualizar a 0.0.6 já instalada pelo botão R; não pedir instalação manual da 0.0.7 antes desse teste. A URL legada responde por redirect do GitHub, sem token. Runtime/cache/saves e políticas de memória mantidos. Não reutilizar o nome antigo do repositório enquanto houver instalações antigas dependentes do redirect. HTTPS e troca no console permanecem pendentes até confirmação.

## Correção 0.0.8 — pacote marcado como atualização

O bloqueio "A base package is required in this version" vinha de Archive::program antes do runtime. O NUP original e o splitado do Orbitals têm TOC íntegra e um único pacote kind=1. O runtime upstream aceita esse caso. Archive::application_title agora prefere a base quando existe e aceita kind=1 da mesma família quando não existe kind=0; DLC isolado ou múltiplas famílias sem base são rejeitados. A seleção é compartilhada por programa e RomFS. Isto não prova que uma atualização arbitrária tenha todos os arquivos.

22 testes de programa, 12 de leitor, 8 de cache, backend nativo e 38 verificações de app passaram localmente. Preparação dos três módulos do Orbitals splitado no PC aprovada; não houve mapeamento, execução de código do jogo, preparação de gigabytes ou gravação em saves. Validar no console após atualizar para 0.0.8 pelo R da 0.0.6/0.0.7.

## Transporte TLS 0.0.9

O teste do updater no console falhou com a mensagem genérica de conexão segura antes de baixar. Foi encontrada chamada indevida da API IPC sslConnectionSetSocketDescriptor com fd POSIX. A correção usa o wrapper socketSslConnectionSetSocketDescriptor, que converte para BSD. DoNotCloseSocket causa retorno -1/ENOENT documentado como sucesso; tratar esse caso explicitamente. O fd duplicado, quando existir, é fechado antes do SSL; o original é fechado pelo dono após SSL.

include/nx2link/tls_socket.hpp contém o adaptador e o erro com etapa/host/Result/errno. tests/tls_mock e test_tls_socket.cpp cobrem contrato ENOENT, retorno de handle e falhas. Fonte de referência: SDK libounce fixado, nx/source/runtime/devices/socket.c e headers ssl.h/socket.h. Não desabilitar validação de certificado/data nem cair para HTTP para contornar falhas. Origem exata do erro observado só será confirmada com nova tentativa no console; não afirmar TLS validado por teste mock.

Instalar a 0.0.9 manualmente, preservando dados, e testar R para consultar a release atual. Após confirmar a consulta no console, publicar uma versão posterior para testar download e reabertura de ponta a ponta. O runtime de jogo não muda.

## Checkpoint 0.0.10

Orbitals abriu no console após a correção de pacote. Não inferir validação de áudio/saves a partir dessa confirmação. A 0.0.10 muda apenas versão e documentação em relação à 0.0.9; preserva a correção TLS, o runtime e os formatos de dados. Teste solicitado: usar R na 0.0.9 já instalada para baixar/instalar 0.0.10 e A para reabrir. Não pedir cópia manual antes desse teste. O teste de TLS/atualização ainda não foi confirmado pela execução do Orbitals.

## Investigação Elliot — 0.0.10

Crash no frontend antes do runtime. menu-operation.log chegou ao SHA da imagem main; launch.log não persistiu essa etapa. O mesmo main tem hash idêntico no PC e os três módulos passam no preflight local. Não concluir que o NUP esteja danificado ou que memória seja a causa sem relatório. Evidência bruta só em local-tests, ignorada no Git.

Branch codex/elliot-fatal-diagnostics, docs/ELLIOT_DIAGNOSTIC.md: build separada, sem alterar release pública. fatal_trace registra heap real e exceções com FS nativo e buffer fixo; instrumentação ligada apenas por --diagnostic. Próximo passo é uma tentativa no hardware e coleta dos relatórios, não publicar uma correção não demonstrada. README da main já foi revisado novamente: rótulos em inglês e ícone no lugar da captura em português.
