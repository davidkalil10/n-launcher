# NX2 Link Enhanced

Homebrew experimental de Switch 2 para preparar e carregar backups NUP diretamente no console. O objetivo Ã© escolher um NUP, preparar os dados uma vez e iniciar o jogo sem depender de um PC para cada jogo.

**Estado atual: prova de leitura, versÃ£o 0.0.1. Ainda nÃ£o relinka, gera RomFS ou executa jogos.** HÃ¡ um leitor C++ compartilhado entre o diagnÃ³stico de PC e o NRO de teste. O trabalho crÃ­tico de carregar o cÃ³digo do jogo continua em pesquisa.

## Entradas desde a primeira versÃ£o

- Arquivo Ãºnico `Jogo.nup`.
- Pasta `Jogo.nup/` contendo `00`, `01`, `02`, ... (FAT32).
- Arquivos `Jogo.nup.00`, `.01`, ... tambÃ©m podem ser inspecionados pelo leitor.

As partes sÃ£o um Ãºnico espaÃ§o lÃ³gico de bytes, com offsets de 64 bits. O leitor cruza fronteiras sem juntar os arquivos. Rejeita Ã­ndices ausentes, partes vazias, tamanho incoerente e TOC corrompida. O inventÃ¡rio verifica SHA-256 da TOC; nÃ£o verifica ainda o conteÃºdo de cada arquivo do jogo. A TOC tem limite explÃ­cito de 64 MiB nesta etapa, sem carregar o payload inteiro na RAM.

## Primeiro teste no console

1. Copie `dist/nx2-link-switch2.nro` para `/switch/nx2-link/` no SD.
2. Crie `/switch/nx2-link/input/` e coloque seu NUP, ou a pasta `.nup` completa, dentro. O navegador permite voltar e localizar arquivos em outras pastas do SD, sem precisar duplicÃ¡-los.
3. Abra pelo hbmenu: direcional seleciona, **A** abre/inspeciona, **B** volta, **+** sai. A pasta `.nup` Ã© tratada como jogo, nÃ£o como pasta para escolher sÃ³ `00`.

Esta versÃ£o apenas lÃª os arquivos. NÃ£o escreve na entrada, nos saves ou na memÃ³ria interna do console. A confirmaÃ§Ã£o de leitura no hardware ainda estÃ¡ pendente.

## Build e testes

No Windows, com g++ disponÃ­vel: `scripts/test-host.ps1 -Compiler <g++.exe>`.

Para gerar o NRO: `scripts/build-switch2.ps1 -SdkRoot <devkitpro> -SupportRoot <DLLs-do-toolchain>`. O script usa libounce/libkaihen, gera link map e verifica o startup OUN0 plataforma 2/versÃ£o 1, ASET, JPEG, NACP e ausÃªncia de bibliotecas Switch 1 incompatÃ­veis. O Ã­cone inicial Ã© o padrÃ£o do SDK.

O GitHub Actions usa fixtures sintÃ©ticas e um SDK pÃºblico verificado, fixado em `config/windows-sdk.json`. A saÃ­da de teste fica em rascunho de release, sem publicaÃ§Ã£o oficial automÃ¡tica. Nenhum jogo Ã© enviado ao CI.

## DocumentaÃ§Ã£o

- [Arquitetura e limites](docs/ARCHITECTURE.md)
- [Roadmap e critÃ©rios de avanÃ§o](docs/ROADMAP.md)
- [DecisÃµes](docs/DECISIONS.md)
- [Aprendizados e evidÃªncias](docs/LEARNINGS.md)
- [Continuidade para outros colaboradores](docs/HANDOFF.md)
- [ReferÃªncia histÃ³rica: bÃ­blia de ports Switch 2](docs/reference/SWITCH2_PORTING_BIBLE.md)

## Upstream e crÃ©ditos

Mantido por **David Kalil Braga (@davidkalil10)**. [Apoiar o desenvolvimento](https://ko-fi.com/davidkalil10).

- [Olmectron/nx2-relink](https://github.com/Olmectron/nx2-relink) e [nx-relink](https://github.com/Olmectron/nx-relink): arquitetura do runtime/relinker, referÃªncia MIT. Base estudada: v0.1.11, registrada em `config/upstream.json`. O runtime ainda nÃ£o foi importado.
- [a55uka/nup](https://github.com/a55uka/nup): formato de container NUP.
- [ouncebrew/kaihen](https://github.com/ouncebrew/kaihen) e [ounce-devkit](https://github.com/ouncebrew/ounce-devkit): ambiente homebrew, carregador e SDK; devkitPro: compilador e ferramentas.
- Eden-NX2 e os ports anteriores de David: documentaÃ§Ã£o de toolchain, memÃ³ria e ciclo de vida. Nenhum cÃ³digo de emulaÃ§Ã£o do Eden foi copiado.

O cÃ³digo prÃ³prio deste projeto Ã© MIT. A documentaÃ§Ã£o de referÃªncia preserva sua autoria; dependÃªncias e futuras importaÃ§Ãµes mantÃªm suas prÃ³prias licenÃ§as. Use backups prÃ³prios. Compatibilidade de um jogo no relinker de PC nÃ£o garante execuÃ§Ã£o nesta homebrew.
