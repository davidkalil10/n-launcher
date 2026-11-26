# Prova de carregamento — 0.0.2

## Teste no console

Use o kaiHEN que já está ativo. Substitua somente a NRO, abra pelo hbmenu e pressione **X**. Não é preciso selecionar NUP, iniciar 4/4, reinstalar ou reativar kaiHEN. A NRO não executa bootstrap nem altera o ambiente homebrew. Se a sessão herdada `ldr:ro` não estiver disponível, o teste informa a ausência e não tenta usar o serviço do sistema como alternativa.

Resultado esperado: **OK: carregador 3/3 ciclos**. Depois, **+** volta ao hbmenu. O navegador e a inspeção NUP continuam disponíveis em **A**, inclusive para o Pokopia splitado e o Elliot.

Registro: `/switch/nx2-link/logs/loader-probe.log`. Cada fase é gravada e fechada antes da próxima chamada importante, para preservar o último ponto alcançado. O registro contém apenas dados técnicos do teste; nenhum módulo de jogo é lido.

## O que a prova faz

Um módulo próprio de 16 KiB é montado durante a build a partir de `fixtures/module.S`, sem imports, chamadas de sistema ou código de jogos. A NRO contém seus bytes como dados. Ao pressionar X, aloca páginas de heap alinhadas a 4 KiB para a imagem e uma página de BSS zerada.

A NRO utiliza a sessão `ldr:ro` já fornecida pelo ambiente para pedir `LoadNro`. Não executa código diretamente do buffer gravável: o kaiHEN mapeia o módulo em outro endereço com as permissões corretas. A NRO verifica cabeçalho R, código RX, constantes R e dados/BSS RW antes de chamar o entrypoint.

O módulo lê uma constante, modifica um contador em dados e outro em BSS e retorna uma conta. A NRO faz duas chamadas, compara os resultados e os contadores, pede `UnloadNro` e só então relê/libera as páginas de origem. Repete tudo três vezes com a mesma identidade de módulo para testar recarga e reinicialização do estado.

## Limpeza e falhas

`ModuleLease` libera o backing apenas antes de uma tentativa de mapear ou depois de um descarregamento confirmado. Uma falha de LoadNro pode deixar mapeamento parcial no serviço; uma falha de UnloadNro também impede liberação segura. Nesses casos, a NRO preserva os 20 KiB de backing e bloqueia novas tentativas nesta instância. O navegador continua utilizável; sair da NRO é o próximo passo. Não tenta ativar kaiHEN novamente.

Nenhum teste de capacidade tenta esgotar os oito slots do loader. A prova usa apenas um módulo por vez. A sessão herdada é emprestada e não é fechada nem reinicializada pelo teste.

## O que uma aprovação significa

Confirma que a sessão atual conseguiu mapear nosso código sintético, executá-lo com acesso a dados/BSS e descarregá-lo repetidamente. Não prova ausência de todo vazamento do serviço, nem relocações de jogos, imports, TLS, NSO/LZ4/ZBIC, serviços gráficos/áudio, saves ou execução de um NUP.

## Referências e licenças

Protocolo/layout conferidos no kaiHEN 20261117.1, commit `3695f364f9270686aa72cc39627bb1281752b2d3`: `ro/source/image.cpp`, `wire.cpp`, `mapping.cpp`, `session.cpp`. O serviço upstream é GPL-2.0-only; sua implementação não foi copiada para o projeto. Também foi consultado o caminho de módulos inteiros do nx2-relink v0.1.11 (MIT). Esta implementação cliente e o módulo sintético são próprios, MIT.

## Validação antes do hardware

- Testes de propriedade com backend simulado: sucesso, erro de carga, erro de descarga, base nula, dupla carga/descarga e recarga. Não substituem o serviço no console.
- Assembler real do SDK gera a imagem; verificador confere layout, constantes, destinos ADR, retorno e ausência de SVC nas instruções do entrypoint.
- NRO principal continua passando pela verificação estrita de startup OUN0, ASET/NACP/JPEG e link map.
- O loader de módulos não usa a API JIT; não se aplica exigir marcador JIT ou criar pool JIT nesta etapa.
