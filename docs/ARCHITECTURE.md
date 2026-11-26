# Arquitetura

## Caminho proposto

`Input (arquivo ou partes) -> índice NUP -> seleção base/update -> preparação RomFS e módulos -> runtime -> kaiHEN ldr:ro -> jogo`

Implementado: Input e inventário de pacotes com TOC verificada. Não implementado: seleção de updates, descompressão de payload, RomFS, preparação NSO, runtime e início do jogo.

## Entrada única e splitada

Toda leitura futura, incluindo zstd, NSO, metadados e RomFS, deve receber Input/read_at, nunca abrir somente a parte 00 como se fosse o jogo inteiro. Partes são ordenadas pelo índice numérico, com início zero e largura consistente. Não se exige tamanho igual das partes; offsets cumulativos de 64 bits permitem divisões arbitrárias. Apenas um arquivo de parte fica aberto durante cada trecho de leitura.

O cabeçalho e a TOC também podem atravessar partes. O tamanho declarado do container precisa corresponder ao total físico; a falta da última parte é detectada mesmo quando os índices restantes são contíguos. A inspeção não é um verificador integral de arquivos ou diretórios internos; limites adicionais serão necessários antes de descompactar ou executar conteúdo.

## Dados do jogo

Gerar RomFS exige descompressão zstd por streaming e construção de índices. O plano é escrever em blocos de até 2 GiB em FAT32 e finalizar um manifesto somente após sucesso. Planejar espaço antes da operação, permitir cancelamento e nunca substituir cache válido por um resultado parcial. O tamanho expandido pode ser maior que o NUP. Montagem direta do NUP sem imagem é pesquisa posterior, não requisito inicial.

## Código executável: hipótese a validar

O builder Python atual gera assembly, tabelas modules.c e um NRO específico usando GCC. O novo projeto não tentará executar esses scripts ou instalar GCC no console. Estudar primeiro um runtime fixo que receba descritores de módulos preparados em C++ e use o mesmo caminho whole-module do upstream via kaiHEN ldr:ro.

Ainda é preciso resolver: NSO LZ4/ZBIC, imports/relocações, TLS, símbolos compartilhados entre módulos, thunks SVC, tabelas dinâmicas, encerramento e jogos que carregam módulos adicionais. O modo whole tem limite de imagens de ldr:ro; alguns jogos exigem modo misto. Isso é o principal risco, não a extensão .nup.

## Memória e ciclo de vida

Não alocar o NUP inteiro nem copiar todas as partes. Limitar TOC, buffers e descompressão. Medir memória do runtime e permissões herdadas do jogo hospedeiro. Configuração de memória por título é explícita e versionada; não transferir automaticamente um valor que abriu um jogo para outros títulos.

Saves ficam em namespace próprio, separado dos saves nativos e dos ports atuais. Um cache por título/build/perfil/runtime deve invalidar só o que mudou. Não reutilizar nomes de pastas como identidade de um jogo.

## O que este projeto não faz

Não converte jogos de Switch 2 em jogos de Switch 1; não emula CPU/GPU como o Eden; não instala títulos no sistema; não modifica NAND; não contorna criptografia; não distribui jogos; não garante áudio, saves ou compatibilidade antes de testes específicos.
