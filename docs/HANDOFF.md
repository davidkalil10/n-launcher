# Continuidade do projeto

## Pedido e objetivo

Preparar/relinkar backups NUP no próprio Switch 2, inclusive pastas .nup com partes 00,01,..., sem precisar do PC para cada jogo. Repositório: https://github.com/davidkalil10/nx2-link-enhanced. Não confundir com nx2-relink-enhanced, que é o builder Windows separado.

## Implementado

Leitor C++ read-only, inventário NUP v1, validação SHA-256 da TOC, CLI de diagnóstico, fixtures sintéticas e frontend console para Switch 2. Scripts de build e verificador estrito do pacote. A versão 0.0.2 também implementa o botão X: prova de módulo sintético via sessão ldr:ro já ativa, com três ciclos e registro persistente. Ainda sem runtime de jogo, execução de jogo, extração ou geração RomFS.

## Fontes úteis

- include/nx2link/input.hpp e src/input.cpp: contrato que TODAS as futuras leituras devem usar.
- src/switch_main.cpp: navegador de diagnóstico somente leitura.
- tests/test_input.py: testes com containers sintéticos, nenhum código de jogo.
- config/upstream.json: base upstream; config/windows-sdk.json: SDK público fixado.
- docs/reference/SWITCH2_PORTING_BIBLE.md: cópia histórica da documentação do Eden, com autoria preservada. Detalhes antigos exigem revisão.

## Próxima investigação

Comparar runtime/nx2.h, whole.c, loader.c, svc_entry.s, svc.c e ro.c com o código gerado por tools/relink.py do upstream fixado. Definir como produzir descritores e imagens no console, onde guardar configuração de jogo hoje compilada via NX2_NAME e como reservar o limite de imagens do loader. Fazer primeiro um módulo sintético e retorno ao menu; só depois tentar o Pokopia.

O teste de Pokopia via PC exibiu imagem com ajuste de memória, mas ficou sem som. O dump de teste atual está splitado. Não presumir que áudio/saves foram validados. Não pedir um NUP não splitado como solução para limitações do leitor.

## Limites operacionais

Não editar o Eden, builder Windows, dumps ou saves. Não enviar jogos/chaves/saves a serviços de build. Não conectar ao console usando um IP antigo sem confirmação da sessão atual. Não apagar caches, backups ou releases de outros projetos. Validar pacote e host tests antes de qualquer teste de execução no hardware.

## Último diagnóstico

Elliot Demo passou na inspeção no console. A pasta Pokopia no SD e a pasta local têm nomes parecidos, mas cabeçalhos e tamanhos diferentes: SD = PFS0/NCA; local = NUP v1 válido. Não reescrever o leitor splitado por esse motivo e não presumir que o conteúdo do SD corresponde ao arquivo que abriu no builder PC. Ver docs/LEARNINGS.md.

## Estado atual do marco de carregamento

Pokopia NUP v1 splitado e Elliot passaram na inspeção no console. Confirmar agora a prova X de 0.0.2 no kaiHEN existente, sem reativá-lo. Ver docs/LOADER_PROBE.md. O módulo sintético não tem imports/TLS/SVC; foi gerado pelo assembler e não é o runtime de jogo. Não avançar a execução do Pokopia com base apenas na build aprovada.
