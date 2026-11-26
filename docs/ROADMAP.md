# Roadmap

## M0 — Leitura e diagnóstico

- [x] NUP único e pasta .nup/00,01,... através do mesmo leitor C++.
- [x] Offsets de 64 bits e leituras cruzando partes, sem recombinar.
- [x] TOC SHA-256, limites de memória e inventário de pacotes.
- [x] Fixtures sintéticas: partes faltantes/vazias, tamanho, hash, limites.
- [x] Fonte do NRO de diagnóstico e build com verificação de startup/link map.
- [x] Elliot Demo inspecionado no console (1 pacote, 49 arquivos).
- [x] Pokopia NUP v1 splitado confirmado no console após nova cópia: 2 partes, 2 pacotes, 41 arquivos.

## M1 — Prova do carregador (decisão de viabilidade)

- [ ] Descrever as interfaces do runtime upstream e o que hoje depende de código gerado.
- [x] Importar o runtime com revisão de licença e hashes, isolando adaptações.
- [x] Implementar prova sintética de carga, permissões, chamadas, dados/BSS, descarga e recarga em três ciclos.
- [x] Prova X confirmada no console: 3/3 ciclos aprovados.
- [x] Preparar NSO LZ4 a partir do NUP completo/splitado, verificar hashes e imagens contra o builder PC.
- [x] Preparação Y do Pokopia confirmada no console em 0.0.3 (3 módulos).
- [ ] Integrar imagens/tabelas ao runtime e gerar thunks; resolver imports, relocações e TLS.
- [ ] Reproduzir a inicialização do Pokopia usando o mesmo NUP e perfil de memória do teste PC.
- [ ] Verificar separadamente imagem, áudio, entradas, saves e retorno ao hbmenu.

Se M1 não puder substituir a etapa de compilação, registrar a limitação antes de investir no launcher completo. Não esconder a dependência do PC.

## M2 — Preparação dos dados

- [x] Descompressão de arquivos em streaming com tamanho e hash verificados (host; hardware pendente).
- [ ] Base/updates e montagem RomFS splitada para FAT32.
- [x] Consulta de espaço, progresso, cancelamento e publicação do staging após verificação (hardware pendente).
- [ ] Cache por identidade do conteúdo + runtime + perfil; retomada só após validação.

## M3 — Produto

- [ ] Biblioteca com capas, títulos, versões e configurações por jogo.
- [ ] Preparar/iniciar a partir de NUP único ou pasta splitada sem diferença no fluxo.
- [ ] Diagnóstico exportável por execução e preservação de saves.
- [ ] Atualização do runtime com migração de cache e reversão.

## Upstream contínuo

- [x] Revisão de referência fixada em config/upstream.json.
- [x] Consulta manual tools/check_upstream.py sem sobrescrever fontes.
- [ ] Matriz de correspondência entre funções Python e implementações no console.
- [ ] Fixtures diferenciais com o builder PC para relocações, seleção de updates e RomFS.

## Integração 0.0.4 — N Launcher

- [x] Biblioteca gráfica com título, ID, tamanho, ícone e indicação de split; pasta persistente.
- [x] Carregador genérico incorporado ao NRO principal; uso da sessão kaiHEN existente.
- [x] Backend nativo de NUP/split e cache de arquivos grandes em partes FAT32, com testes locais.
- [x] Build/verificação de startup e mapa tanto do frontend quanto do loader.
- [ ] Validar no console: interface, cache/progresso e primeira execução do Pokopia.
- [ ] Validar imagem, som, controles, saves e retorno, separadamente.
- [ ] Expor perfil de memória por jogo e gerenciamento de cache.
- [ ] Adicionar preflight ZBIC e formato adicional .nup.00 ao catálogo.
- [ ] Testar corrupção de cache posterior e oferecer revalidação integral opcional.

Itens históricos acima descrevem os marcos completos; integração compilada não fecha a validação de execução.
