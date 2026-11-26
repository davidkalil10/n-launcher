# Decisões

1. **Projeto isolado do Eden e do builder Windows.** Interfaces podem ser integradas depois; provar primeiro o carregamento sem PC.
2. **NUP único e split são requisitos equivalentes.** O Pokopia atual está splitado; suporte a partes não é melhoria opcional.
3. **Preparação em C++ e runtime fixo como hipótese.** Não levar GCC/Python completo ao SD. A hipótese depende da prova M1.
4. **Streaming e offsets de 64 bits.** Evitar cópias gigantes e alocações do tamanho do jogo. A inspeção limita a TOC a 64 MiB e rejeita o restante explicitamente.
5. **Baseline de compatibilidade: kaiHEN 20261117.1.** É a versão validada pelo upstream e pelo toolkit. Outros forks/versões não são equivalentes sem teste.
6. **Acompanhamento manual do upstream.** Cada importação registra commit, licença, arquivos e adaptações. Uma nova versão não substitui automaticamente o runtime em uso.
7. **Estados de validação distintos.** Build aprovada não significa arquivo lido no SD; arquivo lido não significa jogo executado; imagem não significa áudio/saves validados.
8. **Bíblia histórica como referência.** Preservar aprendizados de startup, JIT e ciclo de vida, mas revisar números e regras antigos contra o SDK fixado. Não aplicar recomendações de emuladores indiscriminadamente ao carregador nativo.

9. **Nome público N Launcher.** Preservar nomes internos e caminhos existentes para compatibilidade.
10. **Runtime genérico nx2-flow 0.1.3 e GUI própria sobre seu canvas.** Importação fixada e adaptada por scripts; runtime MIT separado da aplicação gráfica GPL-2.0-or-later. Não copiar recursos ou saves de outros projetos.
11. **Arquivos grandes de RomFS em cache no SD.** Limiar 64 MiB; partes de 1 GiB; SHA de conteúdo e marcador NLC1. A RAM não deve receber o maior arquivo do jogo inteiro.
12. **Nenhuma ativação dentro do NRO.** Handoff pelo hbmenu usa kaiHEN herdado; o carregador vem incorporado e é materializado no SD. O teste de execução é separado da comprovação de build.
13. **Cache de bytes não depende do runtime.** Hash de conteúdo, tamanho e versão do formato identificam dados descomprimidos; não são imagens relinkadas.
