# Validação inicial — 2026-11-26

- 12 testes locais sintéticos aprovados: entrada única/splitada, Unicode, leitura cruzada, partes ausentes/vazias, tamanhos inválidos e SHA-256 com diferentes alinhamentos.
- Leitor Windows compilado com runtime estático. Testado com PATH contendo somente System32, sem dependências DLL do compilador.
- Inspeção somente leitura de um NUP local do Pokopia (7.021.211.961 bytes): dois pacotes, base e atualização 655360; SHA-256 da TOC aprovado. Nenhum payload extraído ou alterado. Este teste não usa a cópia splitada do SD.
- Leitura acima de 4 GiB no NUP local comparada byte a byte com referência Python: aprovada. Nenhuma cópia gigante ou alteração do arquivo de entrada.
- NRO de diagnóstico compilado com devkitA64 r30-1/libounce 20261125-1 e aprovado pelo verificador de startup OUN0, ASET, JPEG, NACP e link map.
- Hardware: leitura no SD pendente. Execução/relink, RomFS, áudio e saves não implementados nesta versão.
- O primeiro teste Windows expôs dependência de DLL e conversão de caminhos com acentos; ambos corrigidos antes de empacotar.

O SHA-256 do NRO acompanha dist/SHA256SUMS.txt (saída de build, não fonte versionado).

## Automação

[GitHub Actions 45751145060](https://github.com/davidkalil10/nx2-link-enhanced/actions/runs/45751145060), commit b47cff5: testes sintéticos no Linux e build/verificação de NRO no Windows aprovados. NRO de teste guardado no rascunho probe-45751145060, sem publicação oficial. SHA-256: `704a339475fdde36dd1519c2fa186adf04b5d3a4e84f4e40835254e9c62f71eb`. Teste no console permanece pendente.

## Prova sintética 0.0.2

- Testes locais de leitura NUP/split: 12 aprovados; testes de propriedade e falha de mapeamento/descarregamento aprovados.
- Módulo sintético: sem relocações pendentes, 14 instruções, referências PC-relativas conferidas, nenhum SVC/import.
- [GitHub Actions 45757116079](https://github.com/davidkalil10/nx2-link-enhanced/actions/runs/45757116079), commit b14a7b0: testes Linux e build/verificação do NRO Windows aprovados.
- NRO local e CI com SHA-256 idêntico: `ac242c2e75bf70abb5e38da5ff18cda1be0a17d96508967cbd99faa256a1ca77`.
- Teste X no console pendente. A implementação usa somente a sessão ldr:ro herdada e não reativa kaiHEN. NUPs e saves não são acessados pelo teste X.

## Preparação 0.0.3

- Teste X de 0.0.2 confirmado no console: 3/3 ciclos, com kaiHEN já ativo.
- 27 testes sintéticos de leitura/preparação aprovados no Windows e Linux, além dos testes de propriedade do carregador.
- Pokopia splitado: imagens main/subsdk0/sdk e metadados comparados com o builder original pelo tools/compare_program.py: idênticos. Nenhum arquivo de jogo modificado.
- [GitHub Actions 45759241537](https://github.com/davidkalil10/nx2-link-enhanced/actions/runs/45759241537), commit a8b292a: testes e NRO aprovados.
- SHA-256 do NRO de teste: `e5396d0b8a877a2537120b4b15d81eae87c492523065ab34962662c0c3ba872c`.
- Preparação Y do Pokopia confirmada no console em 2026-11-26, mantendo o kaiHEN ativo. O jogo não é executado nesta versão.

## Integração experimental 0.0.4

Interface gráfica e runtime genérico incorporado; arquivos grandes de RomFS são preparados para cache no SD. Testes locais incluem 28 testes de leitura/preparação (incluindo update com ID variante 0x800), ownership do carregador, sete casos de cache, três casos do backend nativo com FS simulado e ações/metadados/persistência da GUI. O verificador estrito é aplicado aos dois NROs.

Execução de jogo, framebuffer no console, áudio, saves e retorno ao menu ainda não validados nesta integração. O resultado da prova X e da preparação Y anteriores continua válido para seus próprios escopos.

- [GitHub Actions 45828330300](https://github.com/davidkalil10/nx2-link-enhanced/actions/runs/45828330300), fonte `f66bb0e`: testes Linux e build/verificação Windows aprovados, com rascunho de teste gerado.
- A suíte Linux inclui travessia de parte acima de 4 GiB usando arquivo esparso sintético; esse caso é omitido no Windows para evitar alocação física acidental.
- NRO do CI: 20.104.391 bytes; SHA-256 `52787168534674635e5fb56656dfb72a183da94b2c092840b07a04d476457028`. Download conferido contra o digest da API e SHA256SUMS.txt; NACP confirma N Launcher 0.0.4.
- Prévia da biblioteca no README vem do mesmo renderer da GUI, com fixtures próprias. Nenhum dado de jogo foi enviado ao CI.

## Diagnóstico 0.0.5

- Cache da tentativa de console: 11 marcadores completos e tamanhos correspondentes, sem pastas parciais. A abertura isolada encerrou depois do relatório da imagem main, sem iniciar o runtime.
- Frontend alterado para libounce_core/newlib, com verificação explícita do mapa. Runtime e formato de cache mantidos; causa no alocador ainda é hipótese.
- 28 testes de leitura/preparação e verificação dos NROs aprovados localmente; teste de hardware pendente.
- Pacote N Launcher - Diagnostico 0.0.5: 19.989.703 bytes; SHA-256 `383d5057539d19d76379ca8f6e405502e943ae127e9eebd60c3d7d323cba5475`.

## Confirmação de console 0.0.5 e integração 0.0.6 — 2026-11-26

Pokopia splitado executou com imagem, áudio e controles corretos; salvar e carregar também foram confirmados no console. O frontend newlib é mantido como padrão. A confirmação não cobre outros jogos nem o retorno ao hbmenu.

0.0.6 acrescenta 38 verificações sintéticas para manifesto/HTTP, download interrompido, hash incorreto, cancelamento, troca com cópia anterior, confirmação pela versão nova, perfis e migração sem sobrescrita. Testes da GUI incluem persistência de perfil e confirmação do update. Build local dos dois NROs e mapas aprovada. TLS, migração real e atualização de uma versão para outra no hardware permanecem pendentes.

- [CI da release 0.0.6](https://github.com/davidkalil10/nx2-link-enhanced/actions/runs/45876416386), commit 07434d7: testes Linux, build Windows, verificação dos dois NROs, pacote e publicação aprovados. A build da main e a build da branch também passaram.
- NRO do CI: 20.112.583 bytes; SHA-256 `69ca8dcfa1cf495cad446ac1aa240a6ec28d7f40080ad540fe29af01c09d65d4`. Pacote baixado e hashes da API/SHA256SUMS conferidos; o validador C++ do atualizador aceita o NRO real.
- ZIP de SD: 6.705.024 bytes, SHA-256 `d35220e59da8e1e6f04efe21afceb5b98eb55ace1fab5d86e1b6a43d9b36d49b`; contém somente aplicativo e licenças. Download sem login depende da visibilidade do repositório; não confundir publicação da release privada com feed público acessível.
