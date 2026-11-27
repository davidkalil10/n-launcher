# Elliot — estado da investigação

## Estado atual

Abertura do jogo confirmada no console com a versão normal 0.0.11, sem executar o NRO de diagnóstico. A falha anterior não se repetiu nessa tentativa. Não houve correção específica do preflight entre as duas versões, portanto a causa permanece indeterminada. Áudio, saves e retorno ao hbmenu não têm confirmação específica para este título.

Não é necessário provocar outra tentativa de falha: continuar usando a versão normal. Reservar o diagnóstico para uma recorrência e preservar os registros antes de tentar novamente.

## Evidência anterior

Na 0.0.10, o jogo completo splitado em quatro partes encerrou no frontend antes do runtime. O índice é válido e o hash da imagem principal registrado no SD corresponde ao resultado no PC. O preflight do arquivo do PC passa pelos três módulos. Isso delimita o problema à preparação/registro/progresso no console, sem provar uma causa no alocador ou no NUP.

A evidência bruta foi preservada localmente em local-tests, ignorada pelo Git. Nenhum dump, save, log do SD ou caminho privado foi incluído no repositório. O cache não foi limpo e os arquivos do jogo não foram alterados.

A [branch de diagnóstico](https://github.com/davidkalil10/n-launcher/tree/codex/elliot-fatal-diagnostics) contém um NRO separado, identificado como N Launcher - Diagnostic, com hook de exceção do SDK e registro de std::terminate, heap real e PC/LR/FAR. O writer fatal usa FS nativo, buffer fixo e um arquivo aberto previamente, evitando alocações e stdio nesse caminho. Captura efetiva ainda depende de teste no console.

Se a falha reaparecer, preservar primeiro os logs da versão normal. Depois, usar o diagnóstico em uma tentativa isolada, reabrir FTP e coletar frontend-fatal.log, frontend-fatal.previous.log, launch.log e menu-operation.log antes de outra tentativa. Preservar o ELF/map correspondente ao NRO usado. Não afirmar que instrumentação corrige o crash nem pedir novo dump como primeira medida.

A 0.0.11 acrescenta consulta automática de atualizações, sem incluir o handler de diagnóstico nem afirmar uma correção do Elliot. O teste do diagnóstico mantém a base da 0.0.10, com atualização automática desativada nessa cópia, para não mudar duas variáveis na tentativa.
