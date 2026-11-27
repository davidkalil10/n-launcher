# Continuidade — N Launcher

## Objetivo e estado

Preparar e iniciar NUPs diretamente no Switch 2, incluindo pastas `.nup/00,01,…`, sem compilar um port por jogo no PC e sem reativar kaiHEN para atualizar a NRO. Nome público: **N Launcher**; repositório e namespace C++ nx2-link-enhanced/nx2link são mantidos; a pasta do SD passa a n-launcher em 0.0.6.

**Pokopia splitado foi validado no console em 0.0.5: imagem, som, controles, salvar e carregar funcionam.** O frontend newlib resolveu o fechamento observado; isso não demonstra um defeito geral do SDK. A versão 0.0.6 preserva esse caminho e acrescenta migração, perfis de memória e atualizador. Os recursos novos precisam de validação própria no console, especialmente TLS e troca do aplicativo.

## Organização das fontes

- `src/input.cpp`: leitor comum completo/split e verificação da TOC; offsets cumulativos de 64 bits.
- `src/archive.hpp`: travessia verificada compartilhada, seleção de módulos e overlay de RomFS base/updates.
- `src/program.cpp`: preparação NSO, LZ4, hashes e inventário SVC. Preserva SVC 0x36. Preflight aceita ZBIC desde 0.0.12.
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

PFS0/NCA criptografados não suportados; .nup.00 existe no leitor, mas não no catálogo. Cache publicado confere marcador/tamanhos sem rehash completo; cancelar reinicia o arquivo parcial. Limpeza e revalidação integral de cache ainda sem interface. TLS, atualização efetiva e migração no hardware precisam de teste. Nunca declarar compatibilidade geral a partir de um jogo.

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

## Consulta automática 0.0.11 e investigação Elliot

Menu::begin_startup_check agenda uma consulta por abertura, respeita L → Ajustes e evita duplicar uma consulta manual. Preferência check_updates em menu.cfg, preservada ao escolher a pasta. O worker existente é reutilizado; sucesso sem versão nova, cancelamento e falha automática retornam à biblioteca. Instalação exige confirmação nova. Updates usam update-operation.log para preservar os registros de preparação.

Elliot 0.0.10 encerrou antes do runtime; a imagem main do SD e do PC tem o mesmo hash, e três módulos passam no PC. A causa ainda não foi identificada. Há um NRO separado na branch codex/elliot-fatal-diagnostics e docs/ELLIOT_INVESTIGATION.md descreve o próximo teste. O diagnóstico não foi integrado na release, e a consulta automática não corrige esse crash. Não reiniciar ou reativar kaiHEN automaticamente.

## Confirmação posterior — Elliot 0.0.11

O Elliot abriu no console com a build normal 0.0.11, sem precisar do diagnóstico. Não pedir para trocar uma versão que está funcionando apenas para provocar o crash. Manter a instrumentação separada e recorrer a ela se a falha reaparecer. A causa da falha da 0.0.10 continua indeterminada: não houve correção específica do preflight. Registrar apenas abertura confirmada, sem inferir áudio, saves ou validação do método de atualização usado.

## Pista temporal do crash do Elliot

A tentativa que falhou veio imediatamente após updater → A Reabrir agora. A tentativa que abriu o jogo veio após abrir novamente o aplicativo, sem reboot do console. Registrar como correlação da primeira sessão pós-update, sem atribuir causa ou afirmar correção na 0.0.11. A versão também mudou entre observações, portanto o teste não foi uma comparação controlada da mesma build.

Código já aguarda o worker e chama threadClose antes de oferecer reabertura; rede usa RAII. Não houve falha específica demonstrada para corrigir com segurança. Contorno sugerido: após atualizar, B para hbmenu e abrir manualmente. Não exigir reboot nem provocar outro crash. Se recorrer, comparar o caminho de reabertura com a mesma versão e preservar registros. Updater foi efetivamente utilizado no console, mas a estabilidade após reabertura automática continua sob investigação.

## Preflight ZBIC — 0.0.12

A 0.0.11 recusou o main de Zelda Ocarina of Time, NUP de seis partes, com ZBIC NSO segments are not supported yet. O runtime já continha o decoder. A preparação agora usa o mesmo zbic.c MIT do snapshot nx2-flow 0.1.3; tools/prepare_zbic.py verifica o hash em config/flow.json e gera apenas uma adaptação dos tipos, sem alterar o snapshot. Chamada do transcoder serializada por mutex porque seus buffers de trabalho são estáticos.

O frame convertido ocupa no máximo packed + packed/4 + 64 KiB, com packed limitado a 256 MiB; saída de segmento continua limitada a 256 MiB. Zstandard escreve no destino já limitado, e tamanho/hashes NSO continuam conferidos. Não alterar SVC 0x36 nem escrever imagens de jogo em heap executável.

Testes: amostra sintética real de tabela ZBIC por mixed radix, frame Zstd esperado, concatenação, todas as truncagens da amostra, buffer curto e 1.500 entradas pequenas malformadas. 27 testes de programa e 12 do leitor aprovados. Os três módulos do Zelda splitado passaram no preflight local. Isso ainda não valida execução no console. Nenhum dump/save alterado ou enviado ao CI.

## Estado consolidado após regressão gráfica — 2026-11-27

A investigação está resumida em docs/DISPLAY_TIMEOUT.md. A 0.0.12 preparou e iniciou o Zelda com ZBIC: cutscene com vídeo, áudio inicialmente presente e depois interrompido; após a intro, loading animado por minutos sem gameplay confirmado. A candidata 0.0.13 acrescentava PR44, mas as tentativas paravam no frontend; a 0.0.12 passou a apresentar o mesmo fechamento na sessão ativa, mesmo reabrindo o host sem reboot. Não atribuir isso a reservas persistidas: Pokopia não tinha game.cfg; Elliot/Zelda estavam com configurações vazias. A NRO normal instalada foi conferida contra o SHA público 0.0.12.

Duas builds isoladas de diagnóstico (branch codex/preparation-crash-diagnostic, commits 1d5b5d3 e 074b076) capturaram BadGfxDequeueBuffer (0x2B59), originado em cpuwinWaitImage retornando TimedOut (0xEA01) após 2,418 s, na apresentação da UI enquanto o worker reportava main. Havia mais de 9,4 GB livres no snapshot do heap newlib. O alto InfoType_UsedMemorySize sozinho não indicava OOM. O framebufferBegin do SDK converte esse erro em aborto fatal.

A 0.0.14 desta branch trata aquisição de imagem com timeout sem copiar/avançar/reutilizar a imagem ocupada; o loop pode continuar a preparação e fazer handoff. Pokopia abriu no console com essa build, usando Split Fiction como host e sem reboot/reativação de kaiHEN. A NRO de teste Display Fix foi mantida no SD como alternativa. Outros erros de aquisição resultam em saída após cancelar/join do worker; erros no caminho framebufferEnd não estão cobertos.

A candidata 0.0.15 combina esse frontend com o backoff PR44 fixado em config/runtime-lock-fix.json. Preserva snapshot upstream e base loader 0.1.3; não é atualização integral para loader 0.1.7. CI e script local incluem ambos os testes de buffers/locks. Testes host de cache/backend/app-core/TLS e binários NRO/mapa aprovados. Pacote de teste N Launcher - Sync Test: 20.124.871 bytes, SHA d2975ba998f78c84060ba1211ff94ee371ca3a817e99f83d018d5ec083f9cfc4; loader e1207fabc4d14500b6f88ec85cc0ceb472be5de90abdd1ab0863d5e6c41fbc68. Símbolos retidos em dist/sync-test-symbols.zip. Aguardando FTP para envio separado; ainda sem teste de hardware dessa combinação e sem release pública.

Referência solicitada: https://www.reddit.com/r/switch2hacks/comments/1y605ul/hostdonor_game_table_for_kaihen_nx2flow_nx2relink/ . Valores do Zelda foram conferidos por leitura do próprio main.npdm do NUP local com SHA da TOC/arquivo: GPU 96 MiB, SysRes 48 MiB. A sessão medida no log tinha 64 MiB/40 MiB. Considerar teste controlado de outro host se o loading do Zelda persistir, sem confundir essa diferença com o timeout gráfico do frontend. Não recomendar compra a partir da tabela.
