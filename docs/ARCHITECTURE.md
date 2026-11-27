# Arquitetura

## Componentes e limites de confiança

`Biblioteca gráfica -> Input -> Archive -> preflight dos módulos + cache streaming -> envSetNextLoad -> loader genérico -> ldr:ro -> jogo`

O núcleo de leitura, preparação e cache é MIT. A interface utiliza componentes GPL-2.0-or-later do nx2-flow/Toronja; a aplicação gráfica combinada segue essa licença. O carregador é um componente MIT separado, incorporado à distribuição como NRO. Fontes upstream permanecem originais e verificadas por SHA-256; scripts aplicam alterações explícitas a cópias de build.

## Interface

O mesmo `Menu` desenha uma superfície RGBA 1280×720 no console e nas prévias do PC. O framebuffer e a leitura do controle ficam no driver do Switch. Preparação e escrita do cache ocorrem em thread própria para que o progresso continue visível. Estado de trabalho é sincronizado; sair não libera buffers de uma operação ainda em execução.

A biblioteca reutiliza o leitor de metadados do nx2-flow sobre nosso Input. Pastas `.nup` são entradas de jogo. A invalidação dos metadados considera tamanho e modificação de todas as partes; a verificação antes da execução não depende desse cache visual.

## Entrada e runtime

Input valida a lista de partes, offsets de 64 bits, tamanho declarado e SHA-256 da TOC. Archive faz travessia adicional com limites, detecção de ciclos e seleção de programa/overlay. Não se presume que a parte 00 contenha o arquivo inteiro; cabeçalho, TOC e payloads podem cruzar fronteiras.

No processo do loader, Input é utilizado durante o setup para abrir a mesma lista de partes em handles FS nativos. Depois disso, as threads do jogo usam apenas POD, operações atômicas e `fsFileRead`: STL/C stdio/alocação podem acessar TLS do host e não pertencem a esse caminho. O cache nativo tem a mesma restrição.

O runtime nx2-flow 0.1.3 prepara imagens e descritores dinâmicos, resolve símbolos/relocações/TLS e usa o serviço kaiHEN RO. Seu merge de módulos respeita o limite de sete imagens adicionais. O código de áudio coincide com a referência nx2-relink 0.1.12; essa equivalência de fonte não confirma áudio no console nesta integração.

## RomFS e memória

O loader constrói cabeçalho e tabelas de um RomFS virtual em memória. Arquivos sem compressão vêm diretamente do NUP. Arquivos comprimidos até 64 MiB continuam no cache RAM upstream. Os maiores são preparados com Zstd streaming e SHA-256 para o SD, em partes de 1 GiB, e servidos como entradas kind=3. Não se monta um RomFS gigante persistente e não se carrega o jogo inteiro na RAM.

O preflight de cache aplica base/updates e verifica espaço antes de gravar. A confirmação só é publicada após tamanho e hash aprovados. Saves e dados do runtime têm namespace exclusivo `/switch/n-launcher/<ID>/`; os saves nativos não são abertos para escrita. [Formato do cache](CACHE.md).

## Handoff e ciclo de vida

O NRO principal contém o loader e materializa sua versão na pasta interna `.runtime`. O handoff informa o caminho original do NUP e o NRO do menu para retorno. Não há instalação de título ou nova ativação do kaiHEN. Ownership e permissões do carregamento continuam sob o protocolo upstream; a prova sintética anterior valida somente o seu caminho reduzido de carga/execução/descarga.

Compatibilidade por jogo, áudio, saves e retorno permanecem limites explícitos; o preflight ZBIC foi implementado na 0.0.12. Este projeto não converte jogos de Switch 2 em títulos de Switch 1, não é um emulador de CPU/GPU, não modifica NAND e não distribui jogos ou chaves.
