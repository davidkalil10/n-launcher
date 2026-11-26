# Decisões

1. **Projeto isolado do Eden e do builder Windows.** Interfaces podem ser integradas depois; provar primeiro o carregamento sem PC.
2. **NUP único e split são requisitos equivalentes.** O Pokopia atual está splitado; suporte a partes não é melhoria opcional.
3. **Preparação em C++ e runtime fixo como hipótese.** Não levar GCC/Python completo ao SD. A hipótese depende da prova M1.
4. **Streaming e offsets de 64 bits.** Evitar cópias gigantes e alocações do tamanho do jogo. A inspeção limita a TOC a 64 MiB e rejeita o restante explicitamente.
5. **Baseline de compatibilidade: kaiHEN 20261117.1.** É a versão validada pelo upstream e pelo toolkit. Outros forks/versões não são equivalentes sem teste.
6. **Acompanhamento manual do upstream.** Cada importação registra commit, licença, arquivos e adaptações. Uma nova versão não substitui automaticamente o runtime em uso.
7. **Estados de validação distintos.** Build aprovada não significa arquivo lido no SD; arquivo lido não significa jogo executado; imagem não significa áudio/saves validados.
8. **Bíblia histórica como referência.** Preservar aprendizados de startup, JIT e ciclo de vida, mas revisar números e regras antigos contra o SDK fixado. Não aplicar recomendações de emuladores indiscriminadamente ao carregador nativo.
