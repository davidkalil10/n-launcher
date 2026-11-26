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

## Primeira inspeção no console — 2026-11-26

- Elliot Demo: 1 parte, 1 pacote e 49 arquivos; TOC SHA-256 válida, conforme teste no console.
- Pokopia local: pasta splitada NUP v1 (00 = 4.294.901.760 bytes; 01 = 2.726.310.201 bytes), 2 pacotes e 41 arquivos. Inspeção local e leitura cruzando a fronteira entre partes aprovadas.
- Pokopia no SD: pasta com nome parecido, mas assinatura PFS0 e referências a NCA. 00 = 4.294.901.760 bytes; 01 = 3.003.078.352 bytes. Os tamanhos e as assinaturas demonstram que não é a mesma sequência de bytes da cópia local validada.
- A rejeição pela assinatura não demonstra falha na concatenação das partes. O leitor atual aceita NUP v1; PFS0/NCA exige outro caminho de leitura, não basta tratar o cabeçalho como NUP.
- Não classificar automaticamente PFS0 como arquivo renomeado ou dump inválido. Informar o formato detectado e os limites do leitor.
- Para testar a cópia NUP já validada, copiar a pasta splitada local correta preservando 00/01; não juntar partes nem alterar dumps para contornar a detecção.

## Segunda cópia do Pokopia

A pasta renomeada e recopiada apresentou NUP v1, primeiros 4 KiB iguais ao PC e tamanhos corretos. Inspeção confirmada no console: 2 partes, 2 pacotes, 41 arquivos e TOC SHA-256 válida. A origem da divergência da cópia anterior não foi determinada; não atribuir a uma falha específica do Explorador sem evidência adicional.
