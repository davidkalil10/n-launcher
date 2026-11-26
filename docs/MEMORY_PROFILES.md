# Memória por título

O perfil está em `/switch/n-launcher/<ID>/game.cfg` e vale para os NUPs daquele título, independentemente do nome do arquivo ou da divisão em partes. Configure em **Y → Memória por jogo**, com esquerda/direita, A para salvar e X para restaurar Automático.

| Controle | Faixa | Uso |
|---|---|---|
| Memória do jogo | Automático ou 2048–8992 MiB | Limite informado ao jogo pelo runtime |
| Reserva do carregador | Automático ou 128–2048 MiB | Área inicial para módulos, tabelas e arquivos pequenos descomprimidos |

Automático não grava override de memória do jogo nem passa override de reserva ao loader. O runtime mantém sua política original; a reserva inicial padrão é 384 MiB, além da área do host. A parte não necessária pode ser devolvida quando os módulos e arquivos do título já forem conhecidos.

A reserva usa o argumento numérico `room=<MiB>` sem aspas, pois o ajuste inicial do heap lê a string bruta de argumentos antes do parser normal. O limite do jogo usa o campo `memory` já suportado pelo runtime. Campos como `save`, mods e outros ajustes existentes são mantidos; o perfil usa staging, conferência e recuperação de backup antes da troca.

O perfil é salvo ao configurar/preparar e aplicado ao abrir. Ele não altera a velocidade ou o limite dos buffers fixos usados pelo preparador de cache: há buffers de entrada/saída de 1 MiB e janela Zstd limitada. Arquivos grandes são lidos do cache em partes no SD, sem alocação integral na RAM.

Reservar memória demais ao carregador reduz o que sobra para o jogo. Informar um limite alto demais ao jogo pode fazê-lo reservar mais do que cabe e encerrar na abertura. Não há garantia de mais FPS. O Automático usado no Pokopia foi validado com imagem, áudio, controles e saves; não transferir overrides para outros títulos como se fossem universais.
