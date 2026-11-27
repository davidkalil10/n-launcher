# Interface do N Launcher

Interface própria em português, com biblioteca de seis cartões por página, ícones do NUP, título, ID, tamanho e indicação de arquivo completo ou NUP dividido. Sem ícone disponível, exibe uma capa neutra identificada com N.

Controles: direcional navega; A prepara/inicia; Y abre opções; X atualiza a lista; − escolhe a pasta; + sai. No seletor, A abre pasta, B volta e Y confirma. Pastas `.nup` são jogos e não entram na lista de subpastas. A escolha fica em `/switch/n-launcher/menu.cfg`.

Preparação ocorre em thread própria. A janela de progresso mostra a etapa e, durante o cache, o volume expandido e a fração concluída. B solicita cancelamento; ele só é aplicado em um ponto seguro de preparação, antes do handoff para o jogo.

O renderer de software do nx2-flow/Toronja roda tanto no framebuffer quanto no host. A fonte Noto Sans inclui os acentos usados em português. A geração de prévias usa containers e imagens sintéticos, nunca ícones ou dados de jogos comerciais. `tools/build_gui.py --preview --compiler <g++>` também testa seleção de ações, cancelamento, metadados splitados e persistência da pasta.

Y oferece perfis de memória por título. L abre ajustes do aplicativo, incluindo consulta de updates ao abrir. O gerenciamento de cache ainda não tem interface própria. Prévia correta não substitui validação de novas telas no hardware.

Mensagens preservam quebras de linha e permitem rolagem com o direcional quando o texto excede a janela. O progresso fica indeterminado durante a conferência de módulos e mostra fração durante o cache. Cancelar antes do handoff impede o início do jogo, mesmo se a preparação acabar no mesmo instante.
