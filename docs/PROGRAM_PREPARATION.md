# Preparação de módulos — 0.0.3

## Testar no console

Com o kaiHEN já ativo, substitua a NRO, abra pelo hbmenu, selecione a pasta `.nup` completa do Pokopia (não só 00) e pressione **Y**. O NUP único usa o mesmo fluxo. O teste X de carregamento sintético e a inspeção A permanecem disponíveis.

Resultado esperado para o backup testado: **OK: programa preparado (3 módulos)**. O registro fica em `/switch/nx2-link/logs/program-prepare.log`. A etapa descomprime/verifica módulos em RAM, um por vez, sem gerar arquivo grande no SD e sem persistir as imagens. Não executa o jogo nem escreve nos saves; a ativação do kaiHEN permanece a existente.

## Validação no hardware

A preparação Y do Pokopia foi confirmada no console em 2026-11-26, mantendo a ativação existente do kaiHEN. Esse marco confirma a leitura/descompressão e preparação dos módulos no hardware. A comparação byte a byte com o builder PC foi realizada no host; não houve ainda execução do jogo nem validação de áudio ou saves.

## Implementação

1. Validar TOC e índices de diretórios/arquivos; impedir ciclos, caminhos inválidos e leitura fora dos limites.
2. Identificar a base e selecionar o programa de maior versão do mesmo título com ExeFS/main, ignorando DLC.
3. Ler os arquivos ExeFS através de Input, inclusive cruzando partes. Para codec 1, descomprimir Zstandard em streaming com chunks de 256 KiB e janela limitada a 64 MiB; codec 0 é leitura sem compressão.
4. Conferir o SHA-256 do arquivo NSO completo contra o NUP antes de interpretá-lo.
5. Abrir NSO0, validar segmentos e descomprimir LZ4 (incluindo cópias sobrepostas); verificar hashes de segmento quando os flags os declaram.
6. Preparar a imagem text/rodata/data nos offsets originais, com lacunas zeradas; determinar MOD0, seção dinâmica, module object e limite BSS. Enumerar SVCs sem modificar código, inclusive 0x36, que deve ser preservado no futuro caminho de hooks.
7. Registrar hash da imagem e metadados; liberar o módulo antes de preparar o seguinte.

## Paridade com o builder PC

O preparador C++ foi executado no Pokopia splitado local (2 partes), selecionando a atualização 655360. As três imagens e os metadados MOD0/dynamic/object/end correspondem ao Nso.image() do upstream fixado:

| Módulo | Imagem (bytes) | SHA-256 |
|---|---:|---|
| main | 55.361.216 | 3974c28c9b15ed2e8d79e6e9959543ce5be5b00e32d0de6e25e0f981df150d1e |
| subsdk0 | 1.245.200 | 25428f63986907ec6fe752aae4a1b8158f669c060832a2199ec8fa382676bdf2 |
| sdk | 14.714.992 | c289ed0be1fc30137c050d12f39bdbe5eb288271d0ce731241a8b6b21e338a80 |

O SDK possui 57 instruções SVC identificadas. Nenhum código de jogo ou imagem foi enviado ao CI; só hashes/metadados e fixtures próprias.

## Limites atuais

- NUP v1; PFS0/NCA não é interpretado como NUP. Ainda sem extração de pasta solta.
- É necessário pacote base; DLC e atualizações de outro título não são misturados.
- Arquivo de programa e imagem NSO limitados individualmente a 256 MiB; limite virtual do módulo/BSS de 1 GiB. Isso não define a RAM que será informada ao jogo.
- ZBIC é aceito desde a 0.0.12: o transcoder MIT fixado do nx2-flow reescreve o frame como Zstandard e o decoder valida tamanho e hashes. LZ4 e segmentos sem compressão continuam aceitos, inclusive misturados no mesmo NSO. O formato externo do NUP é independente da compressão interna do NSO.
- Ainda sem relocações/imports/TLS em execução, thunks SVC, runtime do jogo, montagem RomFS ou chamada de nnMain. O botão Y não é um launcher de jogo.
- A leitura pode ocupar alguns segundos por módulo; o progresso é atualizado entre módulos. Cancelamento fino durante a descompressão é melhoria posterior.

## Dependência

Zstandard v1.5.7 oficial, commit f8745da6ff1ad1e7bab384bd1f9d742439278e99. Decoder amalgamado gerado pelo script oficial, com cabeçalhos e licença BSD-3-Clause originais em third_party/zstd. Os fontes de compressão não são incluídos; testes constroem frames sintéticos raw válidos e o backup local verifica o caminho comprimido real.

## Repetir a comparação ao atualizar o upstream

Execute `python tools/compare_program.py --probe <CLI-compilada> --input <NUP-ou-pasta-split> --upstream <checkout-do-builder-PC> --exefs <cache-ExeFS-da-mesma-versao>`. O script apenas lê os dados e compara a imagem/hash, offsets, BSS end e contagem SVC de cada módulo. Usar sempre o cache da mesma atualização; a seleção de versão também é testada com fixtures sintéticas. Uma diferença deve ser revisada antes de alterar a revisão do runtime. O script não envia arquivos a serviços externos.
