# Roadmap

## Concluído

- Leitura NUP completo/splitado por offsets de 64 bits, TOC e limites validados.
- Prova ldr:ro sintética e preparação NSO com comparação ao builder PC.
- Runtime genérico incorporado, kaiHEN existente, cache streaming em partes FAT32.
- Biblioteca gráfica, capas, seleção persistente de pasta, progresso e cancelamento.
- Pokopia splitado validado no console com imagem, áudio, controles, salvar e carregar em 0.0.5.

- Abertura do Orbitals confirmada após suporte a pacote autocontido marcado como atualização; áudio e saves desse título ainda sem confirmação específica.

- Abertura do Elliot completo splitado confirmada na versão normal 0.0.11; causa do crash anterior ainda indeterminada.

## Implementado em 0.0.6 — validar no console

- Perfis de memória por título, mantendo Automático como padrão.
- Migração nx2-link → n-launcher por renomeação, sem recópia do cache.
- Builds automáticas e pacote SD com licenças/manifesto/checksums.
- Atualizador HTTPS com confirmação e cópia anterior até a nova versão iniciar.

- Preflight ZBIC implementado na 0.0.12, com testes sintéticos e preparação do Zelda splitado aprovada no PC; execução desse título no console ainda pendente.

## Próximos passos

- Testar TLS/consulta, troca e confirmação do atualizador no hardware.
- Testar migração e perfis manuais sem alterar os saves existentes.
- Ampliar matriz de jogos e testar retorno ao hbmenu.
- Gerenciar espaço/cache e oferecer revalidação integral opcional.
- Retomada de preparação por bloco, depois de validar integridade.
- Catálogo para partes .nup.00.
- Testes diferenciais adicionais de overlays/relocações com o builder PC.
- Revisar otimizações upstream manualmente, mantendo revisões e licenças fixadas.

O cache depende do conteúdo expandido. Uma troca de nome, perfil ou versão do aplicativo não deve invalidá-lo sem mudança de formato/conteúdo. Histórico dos marcos e resultados em VALIDATION.md e DECISIONS.md.
