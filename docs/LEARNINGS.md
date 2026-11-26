# Aprendizados e evidências

## Pokopia pelo builder de PC

Abertura com imagem confirmada após ativar a opção de memória sugerida. Áudio ausente; causa não diagnosticada. O valor exato usado nessa tentativa deve ser recolhido antes de comparar com o novo carregador. Não registrar o jogo como plenamente compatível.

## Pokopia no Eden-NX2

Os logs analisados mostram execução inicial e apresentação de quadros, mas as amostras de imagem ficaram pretas. Houve mensagens repetidas de textura com formato 0 e encerramento por VK_ERROR_DEVICE_LOST em aproximadamente 141 segundos. A saída SDL de áudio inicializou, sem confirmação de áudio produzido pelo jogo. crash.txt continha histórico acumulado de outras execuções; erros antigos de memória não devem ser atribuídos automaticamente a esta tentativa.

Esses sinais não provam defeito no split. O caminho do Eden emula/traduz gráficos; o relinker aproveita código/SDK do jogo e serviços do console. Não portar o renderer do Eden para este projeto.

## Referências técnicas

- Upstream whole.c mapeia imagens pelo serviço kaiHEN ldr:ro. SVCs de mapeamento de código diretamente pelo guest não são uma alternativa validada.
- O builder PC ainda gera modules.c, assembly e NRO por jogo. O carregador genérico ainda não existe neste projeto.
- Preservar a exceção SVC 0x36 do upstream e verificar limites de módulos, TLS e publicação de código.
- O binário NRO precisa de startup OUN0 plataforma 2/versão 1 real, não apenas da sequência de caracteres em algum ponto do arquivo.
- A TOC e os arquivos podem atravessar limites de partes. Testes não devem assumir que só os dados grandes cruzam fronteiras.
