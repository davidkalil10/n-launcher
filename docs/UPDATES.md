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
