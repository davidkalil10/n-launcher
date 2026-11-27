# ZBIC no preflight

## Problema e implementação

Alguns módulos NSO usam o flag 0x80 e frames com assinatura ZBIC, em vez de blocos LZ4. O runtime nx2-flow fixado já aceita esse formato, mas a preparação anterior ao handoff ainda o recusava. Na 0.0.12, ambos usam o mesmo transcoder original.

`tools/prepare_zbic.py` confere o SHA-256 de loader/runtime/zbic.c contra config/flow.json e gera build/generated/zbic.c. A única adaptação do corpo é trocar o include de plataforma por tipos inteiros portáveis. O snapshot e a licença MIT de Olmectron permanecem preservados. Não baixar uma revisão nova implicitamente durante a build.

O conversor reescreve a representação de probabilidades e o cabeçalho como um frame Zstandard; a descompressão é feita pelo decoder Zstd já utilizado pelo projeto. A conversão é serializada, pois o código original usa buffers estáticos. O mutex protege apenas a conversão; cada chamada mantém seu próprio frame e contexto de descompressão.

## Limites e integridade

- NSO de origem e imagem permanecem nos limites de preparação existentes, até 256 MiB.
- Workspace convertido: packed + packed/4 + 64 KiB, mesmo limite de expansão utilizado pelo runtime; conversão que não cabe é recusada.
- Saída Zstd deve corresponder exatamente ao tamanho declarado do segmento. Erros de frame e hashes inválidos continuam sendo rejeitados.
- Segmentos ZBIC, LZ4 e sem compressão podem coexistir no mesmo NSO.
- O comportamento de BSS, MOD0, SVC e runtime não muda.
- Esse suporte não modifica o NUP nem requer recombinar partes. O leitor comum mantém offsets de 64 bits.

O workspace adicional é por segmento, não por jogo inteiro. A alocação é liberada após a descompressão. Não confundir isso com o cache RomFS, que continua sendo preparado em streaming.

## Validação

A suíte contém uma tabela ZBIC sintética com contagens [16,16], símbolos RLE e uma sequência que gera 100 bytes A, além de frames raw. O resultado convertido é comparado byte a byte com o frame Zstd esperado e depois descomprimido. Há testes de concatenação, truncagem, buffers curtos e 1.500 entradas pequenas malformadas, sem dados de jogos.

A integração NSO verifica hash, tamanho declarado, compressão mista e NUP externo comprimido/splitado. O Zelda Ocarina of Time de seis partes passou pelos três módulos no PC. Execução no console ainda precisa de teste; preparação válida não garante compatibilidade do runtime com todos os serviços do título.
