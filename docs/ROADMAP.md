# Roadmap

## M0 — Leitura e diagnóstico

- [x] NUP único e pasta .nup/00,01,... através do mesmo leitor C++.
- [x] Offsets de 64 bits e leituras cruzando partes, sem recombinar.
- [x] TOC SHA-256, limites de memória e inventário de pacotes.
- [x] Fixtures sintéticas: partes faltantes/vazias, tamanho, hash, limites.
- [x] Fonte do NRO de diagnóstico e build com verificação de startup/link map.
- [ ] Confirmar leitura do Pokopia splitado no console.

## M1 — Prova do carregador (decisão de viabilidade)

- [ ] Descrever as interfaces do runtime upstream e o que hoje depende de código gerado.
- [ ] Importar o runtime com revisão de licença e hashes, isolando adaptações.
- [ ] Carregar um módulo sintético pelo kaiHEN e retornar sem vazamento.
- [ ] Preparar NSO e thunks/tabelas no console; validar módulos inteiros, TLS e imports.
- [ ] Reproduzir a inicialização do Pokopia usando o mesmo NUP e perfil de memória do teste PC.
- [ ] Verificar separadamente imagem, áudio, entradas, saves e retorno ao hbmenu.

Se M1 não puder substituir a etapa de compilação, registrar a limitação antes de investir no launcher completo. Não esconder a dependência do PC.

## M2 — Preparação dos dados

- [ ] Descompressão de arquivos em streaming com tamanho e hash verificados.
- [ ] Base/updates e montagem RomFS splitada para FAT32.
- [ ] Espaço necessário, progresso, cancelamento e gravação transacional.
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
