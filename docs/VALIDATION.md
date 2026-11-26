# Validação inicial — 2026-11-26

- 12 testes locais sintéticos aprovados: entrada única/splitada, Unicode, leitura cruzada, partes ausentes/vazias, tamanhos inválidos e SHA-256 com diferentes alinhamentos.
- Leitor Windows compilado com runtime estático. Testado com PATH contendo somente System32, sem dependências DLL do compilador.
- Inspeção somente leitura de um NUP local do Pokopia (7.021.211.961 bytes): dois pacotes, base e atualização 655360; SHA-256 da TOC aprovado. Nenhum payload extraído ou alterado. Este teste não usa a cópia splitada do SD.
- NRO de diagnóstico compilado com devkitA64 r30-1/libounce 20261125-1 e aprovado pelo verificador de startup OUN0, ASET, JPEG, NACP e link map.
- Hardware: leitura no SD pendente. Execução/relink, RomFS, áudio e saves não implementados nesta versão.
- O primeiro teste Windows expôs dependência de DLL e conversão de caminhos com acentos; ambos corrigidos antes de empacotar.

O SHA-256 do NRO acompanha dist/SHA256SUMS.txt (saída de build, não fonte versionado).
