# N Launcher

Homebrew de Switch 2 para escolher um backup NUP no cartão SD, preparar os dados necessários e iniciar o jogo pelo ambiente kaiHEN já ativo. Aceita **arquivo NUP completo** e **pasta `.nup` com partes `00`, `01`, …** no mesmo fluxo, sem juntar as partes ou compilar um NRO para cada jogo no PC.

**Versão 0.0.4 — build experimental para validação.** A interface gráfica, o carregador genérico e o cache em blocos estão integrados. A execução de jogos por esta versão ainda precisa de teste no console. As etapas anteriores de leitura, prova do carregador e preparação de módulos do Pokopia foram validadas no hardware; isso não confirma imagem, áudio ou saves do novo carregador.

![Biblioteca do N Launcher com dados sintéticos](docs/images/library.png)

*Prévia renderizada com biblioteca fictícia; não representa validação de execução dos jogos.*

## Usar no console

1. Copie `nx2-link-switch2.nro` para `/switch/nx2-link/` no SD. Esse nome de arquivo foi mantido para compatibilidade; o aplicativo aparece como **N Launcher** no hbmenu.
2. Abra pelo hbmenu com kaiHEN ativo. A atualização do NRO não exige refazer a ativação; o aplicativo utiliza a sessão existente.
3. Pressione **−** e escolha a pasta dos jogos. Não é preciso movê-los para uma pasta específica. A escolha fica salva.
4. Selecione o jogo e pressione **A — Jogar**. A preparação mostra progresso e pode ser cancelada com **B** entre etapas seguras. A primeira preparação pode levar vários minutos.

**Y — Opções** oferece preparação sem executar, verificação do índice, teste sintético do carregador e registros. **X** atualiza a biblioteca; **+** sai. A pasta `.nup` é um jogo completo para o seletor: não escolha apenas a parte `00`.

O NRO inclui seu próprio carregador e o grava em uma pasta interna do aplicativo no SD quando necessário. Nenhum compilador, Python ou DLL precisa ser instalado no console. Ele não ativa kaiHEN, não instala jogos no sistema e não altera os saves nativos.

## Espaço no cartão e dados preservados

Arquivos comprimidos de RomFS maiores que 64 MiB são descomprimidos para um cache próprio no SD, em partes de até **1 GiB**, com SHA-256 conferido antes da publicação. Isso permite servir arquivos maiores que a memória disponível ao carregador. O espaço necessário é consultado antes de iniciar a gravação; o cache expandido pode ocupar vários gigabytes além do NUP.

- NUPs e suas partes são somente leitura.
- Saves do runtime ficam em `/switch/nx2-link/<ID>/save`, separados dos saves do console e de outros projetos. Compatibilidade de saves ainda requer validação nesta versão.
- `/switch/nx2-link/content/` contém apenas cache de dados expandidos. Uma preparação cancelada pode deixar uma pasta `.partial`, reaproveitada como área de trabalho na próxima tentativa; a descompressão recomeça do início.
- `/switch/nx2-link/cache/` guarda os metadados e ícones da biblioteca; não é o cache expandido do jogo.
- Registros da preparação ficam em `/switch/nx2-link/logs/`; registros do jogo ficam na pasta do respectivo ID.

Cache publicado só é reutilizado quando marcador, tamanho e partes conferem. A reutilização não relê todos os bytes para detectar alterações posteriores no SD. [Formato, recuperação e limites](docs/CACHE.md).

## Compilar e verificar

Os requisitos a seguir são somente para desenvolvimento no PC:

```powershell
python -m pip install -r requirements-build.txt
./scripts/test-host.ps1 -Compiler <g++.exe>
./scripts/test-launcher.ps1 -Compiler <g++.exe>
./scripts/build-switch2.ps1 -SdkRoot <devkitpro> -SupportRoot <DLLs-do-toolchain>
```

A build verifica fontes upstream fixadas, gera o carregador, incorpora-o à interface e valida os dois NROs: startup OUN0 plataforma 2/versão 1, ASET, JPEG, NACP e link map sem bibliotecas Switch 1 incompatíveis. O SDK público está fixado em `config/windows-sdk.json`.

O GitHub Actions utiliza apenas fontes públicas e fixtures sintéticas, sem jogos, chaves ou saves. Builds de teste são guardadas em rascunhos, sem publicação oficial automática. `tools/build_gui.py --preview --compiler <g++>` renderiza a interface com uma biblioteca fictícia e testa suas ações.

## Arquitetura e continuidade

- [Arquitetura](docs/ARCHITECTURE.md)
- [Cache em blocos](docs/CACHE.md)
- [Interface gráfica](docs/INTERFACE.md)
- [Roadmap e validação](docs/ROADMAP.md)
- [Decisões](docs/DECISIONS.md) e [aprendizados](docs/LEARNINGS.md)
- [Handoff técnico](docs/HANDOFF.md)
- [Prova sintética](docs/LOADER_PROBE.md) e [preparação de módulos](docs/PROGRAM_PREPARATION.md)

## Créditos e licenças

Mantido por **David Kalil Braga (@davidkalil10)**. [Apoiar o desenvolvimento](https://ko-fi.com/davidkalil10).

- [Olmectron/nx2-flow](https://github.com/Olmectron/nx2-flow): carregador genérico 0.1.3, MIT; componentes gráficos, metadados e imagens do menu 0.2.2, GPL-2.0-or-later. Revisão e hashes em `config/flow.json`. As adaptações são geradas separadamente da cópia original.
- [Olmectron/nx2-relink](https://github.com/Olmectron/nx2-relink) e [nx-relink](https://github.com/Olmectron/nx-relink): runtime e relinker MIT. A referência 0.1.12 com correção de áudio está preservada em `config/upstream.json`; a execução usa o runtime genérico do nx2-flow.
- [Toronja](https://github.com/Olmectron/toronja), Azahar Emulator Project e PalindromicBreadLoaf: origem dos componentes gráficos reutilizados pelo nx2-flow; avisos GPL preservados nos arquivos.
- [a55uka/nup](https://github.com/a55uka/nup): formato NUP.
- [ouncebrew/kaihen](https://github.com/ouncebrew/kaihen), [ounce-devkit](https://github.com/ouncebrew/ounce-devkit) e devkitPro: ambiente homebrew, serviço de carregamento, SDK e ferramentas.
- [Zstandard](https://github.com/facebook/zstd): decoder 1.5.7, BSD-3-Clause.
- [Noto Sans](https://github.com/google/fonts/tree/main/ofl/notosans): fonte da interface, SIL Open Font License 1.1. Pillow é utilizado apenas na geração de fontes, ícones e prévias durante a build.
- Eden-NX2 e os ports anteriores de David: referências de toolchain e ciclo de vida. Nenhum código de emulação do Eden foi copiado.

O núcleo próprio de leitura/preparação/cache e o runtime MIT mantêm suas licenças permissivas. A interface gráfica e a aplicação que a incorpora são distribuídas sob **GPL-2.0-or-later**, com as fontes correspondentes neste repositório. As licenças completas e os avisos das dependências estão em `third_party/`. Use backups próprios; compatibilidade no relinker de PC não garante compatibilidade nesta homebrew.
