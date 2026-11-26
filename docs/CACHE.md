# Cache de arquivos grandes

## Motivação

O loader genérico upstream mantém um arquivo comprimido inteiro na RAM após descompressão. Isso não atende a arquivos individuais maiores que sua área de memória. O N Launcher descomprime os arquivos de RomFS maiores que 64 MiB para o SD, sem juntar o NUP splitado e sem criar um RomFS gigante.

## Layout versão 1

```text
/switch/n-launcher/content/<sha256>/
  0000.bin
  0001.bin
  ...
  complete
```

Cada parte tem até 1 GiB; a última contém o restante. `complete` tem 48 bytes: `NLC1` (4), tamanho de parte u32 LE (4), tamanho expandido u64 LE (8), SHA-256 do conteúdo (32). O nome da pasta usa o mesmo SHA do arquivo na TOC do NUP. O cache contém bytes originais descomprimidos, sem patch ou relink; por isso sua identidade não depende da versão do runtime.

## Preparação

1. Validar Input e TOC; montar overlay de RomFS base/updates do mesmo título.
2. Separar arquivos comprimidos grandes, deduplicar por hash e identificar caches completos existentes.
3. Limpar apenas as partes conhecidas da área de staging interrompida do próprio cache. Não há remoção recursiva de pastas.
4. Conferir espaço para os dados faltantes, com margem de 32 MiB.
5. Ler/descomprimir em buffers de 1 MiB, com janela Zstd limitada a 64 MiB; gravar partes de 1 GiB e calcular SHA incremental. Frames com bytes adicionais, tamanho incorreto ou hash diferente são recusados.
6. Fechar as partes, escrever o marcador e renomear `<sha>.partial` para `<sha>`. Um cache final existente e inválido não é sobrescrito automaticamente.

Cancelar entre etapas deixa apenas staging incompleto; a próxima tentativa refaz a descompressão, sem retomada no meio de um frame. Cache final válido e NUPs permanecem intactos. O protocolo evita publicar um resultado conhecido como incompleto; não é uma garantia contra corrupção física do SD ou perda de energia.

## Uso pelo jogo

Antes do jogo, o loader confere marcador, hash declarado, tamanho e presença de todas as partes. As leituras usam offsets u64, atravessam partes e mantêm poucos handles em LRU. Esse caminho não usa alocação, STL ou C stdio nas threads do jogo.

O SHA é calculado durante a criação. Reutilizar o cache confere marcador e tamanhos, sem rehash dos dados; uma alteração posterior que preserve o tamanho pode passar despercebida. Se houver suspeita de corrupção, preserve os logs e remova apenas a pasta de hash afetada para preparar novamente. Não remova pastas de saves ou o NUP como tentativa de correção.

## Validação e pendências

Fixtures sintéticas cobrem streaming de frames RLE/raw, fronteiras de entrada/saída e de partes, reuso, hash incorreto, cancelamento, nova tentativa, bytes adicionais e cache incompleto. O backend usado pelo loader é compilado no host com FS simulado para testar suas leituras e limites.

Desempenho, espaço e consumo de memória em jogo real precisam de medição no console. Estão pendentes gerenciamento visual de cache, limpeza de itens não usados e verificação integral opcional de caches já publicados.
