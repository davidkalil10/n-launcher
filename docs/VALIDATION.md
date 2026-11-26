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
- Preparação Y no console ainda pendente. O jogo não é executado nesta versão.
