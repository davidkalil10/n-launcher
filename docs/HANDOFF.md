# Continuidade — N Launcher

## Objetivo e estado

Preparar e iniciar NUPs diretamente no Switch 2, incluindo pastas `.nup/00,01,…`, sem compilar um port por jogo no PC e sem reativar kaiHEN para atualizar a NRO. Nome público: **N Launcher**; repositório e caminhos internos nx2-link-enhanced/nx2link/nx2-link são mantidos.

**0.0.4 é uma integração experimental, pendente de teste de jogo no console.** M0/leitura, prova sintética X (3/3) e preparação Y dos três módulos do Pokopia em 0.0.3 já foram confirmados no hardware. Não solicitar outra prova equivalente como substituição de avanço real. Não afirmar que o jogo, áudio ou saves funcionam no runtime novo sem confirmação.

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

O frontend inclui o NRO genérico como recurso. O usuário copia somente o NRO principal. O carregador é materializado em `/switch/nx2-link/.runtime/`, com arquivo temporário e preservação da versão anterior durante a troca. Isso não instala ou atualiza kaiHEN. A chamada do carregador usa a sessão herdada do hbmenu.

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

Copiar a build de teste para o SD, abrir no hbmenu existente, escolher a pasta com o NUP splitado correto e usar Preparar ou Jogar. A primeira preparação pode levar minutos e exige espaço extra no SD. Confirmar visual, navegação e progresso; depois observar o handoff e a inicialização do jogo. Se houver erro, coletar `/switch/nx2-link/logs/menu-operation.log` e a pasta `<ID>/` (log/crash), mantendo os arquivos originais preservados.

Não acessar um IP antigo do console sem confirmação da sessão atual. Não reiniciar ou reativar kaiHEN automaticamente. Uma falha depois do handoff pode encerrar o processo ou a sessão: o retorno ao menu depende do runtime e ainda precisa de validação no hardware.

## Limitações abertas

Execução, áudio, saves e retorno ainda pendentes nesta integração; preflight ZBIC não implementado; formatos criptografados PFS0/NCA não suportados; suporte adicional `.nup.00` está no leitor, mas não no catálogo gráfico. A reutilização de cache confere marcador e tamanhos, sem rehash completo. Cancelar refaz o arquivo de staging na próxima tentativa, sem retomada por bloco. Perfil de memória e gerenciamento/limpeza de cache precisam de interface própria. Não publicar uma release estável apenas porque a build passou.
