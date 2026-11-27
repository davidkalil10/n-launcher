# Atualização e releases

## No console

Instale a primeira versão com atualizador pelo SD. Em seguida, **R — Atualizações** consulta `releases/latest/download/update.ini` do repositório oficial. A consulta só acontece quando solicitada; não interrompe o jogo nem baixa atualizações silenciosamente.

O repositório e os assets precisam estar acessíveis publicamente para o console: este fluxo não armazena credenciais do GitHub. Uma release publicada em repositório privado pode ser baixada manualmente com a conta autorizada, mas retorna 404 ao atualizador sem login.

O manifesto versão 1 contém schema, versão, nome do NRO, tamanho e SHA-256. O aplicativo aceita somente uma versão numérica estável mais recente, o asset `n-launcher.nro`, tamanho limitado e HTTPS para GitHub/CDNs explicitamente permitidas. Certificado, hostname e data são verificados; não existe fallback HTTP sem TLS. Falhas de rede ou de relógio são mostradas na interface.

Após a confirmação, o download vai para um arquivo temporário ao lado do NRO em execução, em blocos. Tamanho, hash recebido, hash relido do SD, NRO0/MOD0/OUN0, plataforma Switch 2, assets, nome e versão são conferidos antes da publicação. O aplicativo é atualizado onde foi aberto; jogos, cache, perfis e saves não são destinos do atualizador.

A versão anterior fica com sufixo `.previous`, acompanhada de `.update-state`. Depois da troca, A reabre o aplicativo; B volta ao hbmenu. A nova versão só remove esses arquivos após conferir versão, hash e estrutura do NRO instalado. O processo antigo não pode confirmar a versão nova. Um erro antes da troca mantém o NRO atual; erro na segunda renomeação tenta restaurar a cópia anterior.

## Recuperação

Em caso de interrupção antes da troca, tente atualizar novamente; o temporário é descartável. Se houver perda de energia justamente entre as renomeações e o NRO principal ficar ausente, restaure o arquivo `.previous` ao nome original pelo FTP/SD. Os dados do aplicativo permanecem separados. Não é uma promessa de instalação atômica resistente a qualquer falha física do cartão.

Não apague saves ou cache para corrigir um download. Um `.previous` sem estado reconhecido não é removido automaticamente. Abrir um arquivo enviado apenas por Netloader pode não oferecer um destino permanente; use o NRO instalado no SD.

## Build no GitHub

O workflow testa leitor, NSOs, ownership, cache, backend nativo simulado, protocolo de atualização, preservação/rollback, perfis, migração e ações da GUI. Depois gera runtime e frontend com SDK fixado, verifica os dois NROs e empacota:

- `n-launcher.nro`: aplicativo com loader incorporado;
- `N-Launcher-v<versão>-Switch2.zip`: estrutura para SD e licenças;
- `update.ini`: feed de atualização daquela release;
- `SHA256SUMS.txt`: hashes dos assets.

Pushes normais geram rascunhos de teste, excluídos do feed público. Para publicar, atualize `NX2LINK_VERSION` no código, integre na main e faça push da tag correspondente, por exemplo `v0.0.7`. Também é possível executar o workflow na main com **publish_release=true**. A tag precisa corresponder à versão do aplicativo; commits de branches não integradas não podem ser publicados como release.

A release é preparada como rascunho e só se torna pública/latest após o upload completo dos assets. Não substitua assets de uma versão pública: publique uma nova versão. Cada importação de runtime upstream continua exigindo revisão explícita; o auto updater atualiza o pacote do N Launcher, não o kaiHEN nem o firmware.

## Validação

Os testes usam apenas dados sintéticos e transporte simulado. Incluem HTTP com respostas fragmentadas e chunked, redirecionamento, limites, manifesto inválido, interrupção, cancelamento, hash incorreto, plataforma/versão erradas e confirmação do backup. HTTPS nativo e uma troca completa no console ainda precisam de confirmação específica em hardware.

## Teste 0.0.6 → 0.0.7

O repositório oficial passou a ser `davidkalil10/n-launcher` e está público. A 0.0.6 instalada usa o endereço anterior, `davidkalil10/nx2-link-enhanced`; o GitHub redireciona suas URLs de release para o novo nome. Mantenha o nome antigo sem reutilização em outro repositório para preservar esse acesso das versões já instaladas.

Pressione R na 0.0.6, confirme a 0.0.7, aguarde instalar e pressione A para reabrir. Confira a versão no canto superior. O novo processo confere o arquivo e confirma a atualização antes de remover sua cópia anterior. Nenhum novo cache de jogo é exigido por esta versão.

## Correção de transporte 0.0.9

A 0.0.6 mostrou falha genérica de conexão segura no console. A implementação usava sslConnectionSetSocketDescriptor diretamente com um fd POSIX, contrariando a orientação do SDK. A 0.0.9 usa socketSslConnectionSetSocketDescriptor para traduzir o handle. Com DoNotCloseSocket, retorno -1/ENOENT é sucesso documentado e não uma falha. Outros erros mostram etapa/host/Result ou errno. CA, hostname e data continuam verificados.

É necessário substituir o NRO manualmente para aplicar esta correção em uma versão cujo download falha. Depois, testar R na 0.0.9: a consulta deve reconhecer a versão atual. Não publicar uma versão vazia apenas para forçar troca antes de validar a consulta TLS no console. Confirmada a consulta, preparar uma versão posterior para testar download/troca/reabertura. Nada nesta correção requer reinstalar o kaiHEN ou refazer o cache.

## Teste 0.0.9 → 0.0.10

Com a correção TLS da 0.0.9 instalada, pressione R, confirme 0.0.10, aguarde instalar e pressione A para reabrir. Conferir a versão exibida e a confirmação do novo processo. A mudança 9 → 10 é comparada numericamente; a suíte cobre esse caso. O código de execução permanece o mesmo, para separar o teste do atualizador de mudanças no jogo. Ainda é necessária validação de cada etapa no hardware.

## Consulta automática — 0.0.11

Uma consulta é agendada ao abrir o aplicativo e usa o worker existente antes de permitir outra operação concorrente. A opção padrão é ligada, com preferência check_updates=0/1 em menu.cfg. L abre Ajustes para alterá-la; R permanece manual, inclusive com a consulta ao abrir desativada. Alterar a pasta preserva a preferência e campos desconhecidos.

A consulta usa a janela de progresso existente e permite solicitar cancelamento com B. Não instala automaticamente. Resultado sem versão nova ou erro retorna à biblioteca; erros de rede mostram aviso discreto e podem ser consultados novamente com R. Uma versão nova abre a confirmação e exige um novo A após sua exibição. A consulta manual na mesma abertura consome o agendamento automático, evitando duas requisições seguidas.

Atualizações gravam logs/update-operation.log. Os registros de preparação e abertura não são substituídos pela consulta ao iniciar. Não confundir a execução do Orbitals com validação de download/troca do aplicativo no console. O diagnóstico do Elliot continua separado desta mudança.

## Observação de estabilidade após reabrir

Há um relato de encerramento na preparação do Elliot na primeira sessão após atualizar e usar A Reabrir agora. Uma abertura posterior funcionou sem reboot. A relação causal continua em investigação; ver ELLIOT_INVESTIGATION.md. Como contorno, é possível usar B após instalar, voltar ao hbmenu e abrir manualmente. O código não foi alterado com base apenas nessa correlação.
