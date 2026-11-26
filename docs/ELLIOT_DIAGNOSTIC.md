# Elliot: diagnóstico de encerramento no frontend

## Evidência preservada

Na versão 0.0.10, a abertura de The Adventures of Elliot completo, splitado em quatro partes, encerrou durante o preflight. O último launch.log termina em Preparing main. menu-operation.log contém o relatório de hash da imagem principal, indicando que sua preparação chegou ao cálculo de SHA-256. Esse hash corresponde ao resultado do mesmo arquivo no PC. A passagem por todos os três módulos no PC foi aprovada. Isso não valida a execução no console nem identifica sozinho a causa do encerramento.

Não houve entrada no runtime nessa tentativa. Não gerar novamente NUPs ou limpar cache como primeira medida. Nenhum jogo, save ou arquivo de log do SD faz parte deste commit. Os registros originais ficam apenas na pasta local ignorada de diagnóstico.

## Build separada

`tools/build_gui.py --diagnostic` gera dist/n-launcher-diagnostic.nro com nome N Launcher - Diagnostic no hbmenu. A versão do código continua 0.0.10; esta build não é uma nova release e desativa seu próprio botão de atualização. A imagem inclui o mesmo runtime e formato de cache da versão normal. O build normal compila as chamadas de instrumentação como funções vazias e não instala o handler.

Somente na build de diagnóstico:

- Mantém um arquivo de relatório aberto antes do trabalho pesado, usando FS nativo para o registro fatal; não usa malloc, stdio nem formatação dependente de locale no escritor fatal.
- Registra etapas de worker/UI e medidas reais de heap do SDK, distintas da memória do processo já reservada.
- Captura std::terminate (incluindo bad_alloc) e a exceção nativa via hook oficial do SDK, com PC/LR/FAR/ESR e endereço-base para comparação ao ELF/map.
- Usa stack de exceção separada de 32 KiB. Não tenta recuperar execução após uma falha: ao retornar, o trampoline padrão do SDK segue seu fluxo fatal.
- Escreve logs/frontend-fatal.log e preserva o anterior em frontend-fatal.previous.log. Um diagnóstico encerrado normalmente recebe marcador diagnostic_finished.

A captura efetiva de uma falha no console ainda precisa de teste; compilação não comprova que todos os tipos de encerramento serão interceptados. Não afirmar que esta instrumentação corrige o crash. Falhas no próprio serviço FS ou encerramentos externos podem impedir a gravação.

## Próximo teste

Copiar o NRO com nome separado para a pasta do N Launcher, preservando o NRO normal. Fechar FTP, abrir N Launcher - Diagnostic e tentar somente o Elliot uma vez. Se abrir, confirmar o resultado; se encerrar, reabrir o FTP e coletar frontend-fatal.log, frontend-fatal.previous.log, launch.log e menu-operation.log antes de outra tentativa. Não reiniciar ou reativar kaiHEN automaticamente. O ELF/map correspondente está em build/gui-diagnostic e precisa ser preservado para simbolizar PC/LR.

A branch codex/elliot-fatal-diagnostics faz a build separada no Actions e a guarda no rascunho de teste. Não promove o diagnóstico ao feed público de atualizações.
