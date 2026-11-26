# NX2 Link Enhanced

Homebrew experimental de Switch 2 para preparar e carregar backups NUP diretamente no console. O objetivo é escolher um NUP, preparar os dados uma vez e iniciar o jogo sem depender de um PC para cada jogo.

**Estado atual: prova de leitura e carregador sintético, versão 0.0.2. Ainda não relinka, gera RomFS ou executa jogos.** Há um leitor C++ compartilhado entre o diagnóstico de PC e o NRO de teste. O botão X testa um módulo sintético pela sessão kaiHEN já ativa; o trabalho de carregar o código de jogos continua em pesquisa.

## Entradas desde a primeira versão

- Arquivo único `Jogo.nup`.
- Pasta `Jogo.nup/` contendo `00`, `01`, `02`, ... (FAT32).
- Arquivos `Jogo.nup.00`, `.01`, ... também podem ser inspecionados pelo leitor.

As partes são um único espaço lógico de bytes, com offsets de 64 bits. O leitor cruza fronteiras sem juntar os arquivos. Rejeita índices ausentes, partes vazias, tamanho incoerente e TOC corrompida. O inventário verifica SHA-256 da TOC; não verifica ainda o conteúdo de cada arquivo do jogo. A TOC tem limite explícito de 64 MiB nesta etapa, sem carregar o payload inteiro na RAM.

## Primeiro teste no console

1. Copie `dist/nx2-link-switch2.nro` para `/switch/nx2-link/` no SD.
2. Crie `/switch/nx2-link/input/` e coloque seu NUP, ou a pasta `.nup` completa, dentro. O navegador permite voltar e localizar arquivos em outras pastas do SD, sem precisar duplicá-los.
3. Abra pelo hbmenu: direcional seleciona, **A** abre/inspeciona, **B** volta, **X** testa o carregador sintético, **+** sai. A pasta `.nup` é tratada como jogo, não como pasta para escolher só `00`.

A inspeção apenas lê os arquivos. O teste X escreve seu próprio registro em `/switch/nx2-link/logs/loader-probe.log`; não escreve na entrada, nos saves ou na memória interna. Ele usa a ativação kaiHEN existente, sem reiniciar ou refazer 4/4. A demo do Elliot passou na inspeção no console (1 pacote, 49 arquivos). A cópia NUP v1 splitada do Pokopia passou no leitor local; a cópia examinada no SD era outro container, PFS0/NCA, ainda não suportado. Consulte docs/LEARNINGS.md.

## Build e testes

No Windows, com g++ disponível: `scripts/test-host.ps1 -Compiler <g++.exe>`.

Para gerar o NRO: `scripts/build-switch2.ps1 -SdkRoot <devkitpro> -SupportRoot <DLLs-do-toolchain>`. O script usa libounce/libkaihen, gera link map e verifica o startup OUN0 plataforma 2/versão 1, ASET, JPEG, NACP e ausência de bibliotecas Switch 1 incompatíveis. O ícone inicial é o padrão do SDK.

O GitHub Actions usa fixtures sintéticas e um SDK público verificado, fixado em `config/windows-sdk.json`. A saída de teste fica em rascunho de release, sem publicação oficial automática. Nenhum jogo é enviado ao CI.

## Teste do carregador

Pressione **X** sem selecionar jogo. Resultado esperado: **OK: carregador 3/3 ciclos**. [Instruções e limites](docs/LOADER_PROBE.md). A validação no hardware desta etapa ainda está pendente.

## Documentação

- [Prova de carregamento](docs/LOADER_PROBE.md)
- [Arquitetura e limites](docs/ARCHITECTURE.md)
- [Roadmap e critérios de avanço](docs/ROADMAP.md)
- [Decisões](docs/DECISIONS.md)
- [Aprendizados e evidências](docs/LEARNINGS.md)
- [Continuidade para outros colaboradores](docs/HANDOFF.md)
- [Referência histórica: bíblia de ports Switch 2](docs/reference/SWITCH2_PORTING_BIBLE.md)

## Upstream e créditos

Mantido por **David Kalil Braga (@davidkalil10)**. [Apoiar o desenvolvimento](https://ko-fi.com/davidkalil10).

- [Olmectron/nx2-relink](https://github.com/Olmectron/nx2-relink) e [nx-relink](https://github.com/Olmectron/nx-relink): arquitetura do runtime/relinker, referência MIT. Base estudada: v0.1.11, registrada em `config/upstream.json`. O runtime ainda não foi importado.
- [a55uka/nup](https://github.com/a55uka/nup): formato de container NUP.
- [ouncebrew/kaihen](https://github.com/ouncebrew/kaihen) e [ounce-devkit](https://github.com/ouncebrew/ounce-devkit): ambiente homebrew, carregador e SDK; devkitPro: compilador e ferramentas.
- Eden-NX2 e os ports anteriores de David: documentação de toolchain, memória e ciclo de vida. Nenhum código de emulação do Eden foi copiado.

O código próprio deste projeto é MIT. A documentação de referência preserva sua autoria; dependências e futuras importações mantêm suas próprias licenças. Use backups próprios. Compatibilidade de um jogo no relinker de PC não garante execução nesta homebrew.
