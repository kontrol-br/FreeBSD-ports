# Kontrol OpenVPN Schedule Package

## Visão geral
Package para controle de login por horário **somente no OpenVPN**, reaproveitando os schedules do pfSense/Kontrol.

## Instalação
1. Instale o port `www/Kontrol-pkg-OpenVPN-Schedule` normalmente via pkg/ports.
2. O `POST-INSTALL` chama `/etc/rc.packages` e executa `openvpn_schedule_install()`.
3. O instalador valida compatibilidade do arquivo alvo e aplica patch idempotente com backup + checksum.

## Configuração
1. Acesse **VPN > OpenVPN Schedule**.
2. Defina a política default (`allow` recomendado).
3. Cadastre regras no formato `tipo,nome,schedule`:
   - `user,alice,Comercial`
   - `group,vpn-users,Noite`
4. Prioridade de decisão:
   1. regra de usuário
   2. regra de grupo
   3. política default

## Como testar
1. Crie/valide schedules em **System > Routing > Schedules**.
2. Teste autenticação OpenVPN dentro do horário permitido.
3. Teste autenticação OpenVPN fora do horário permitido.
4. Verifique logs: mensagens com tag `[openvpn_schedule]`.

## Desinstalação / rollback
1. Remova o package.
2. O `pkg-deinstall` executa `openvpn_schedule_deinstall()`.
3. O deinstall restaura o arquivo original a partir de backup validado por checksum.
4. Se checksum/backup falhar, rollback é abortado com log explícito para recuperação manual.

## Upgrade
- `POST-UPGRADE` executa migração de configuração e reaplica patch idempotente.
- Backups e manifesto (`/var/db/openvpn_schedule/manifest.json`) são mantidos consistentes.

## Limitações conhecidas
- O patch runtime depende de âncoras conhecidas em `/etc/inc/openvpn.auth-user.php`.
- Se o arquivo base mudar de forma incompatível, a instalação é abortada com erro de compatibilidade (comportamento seguro).
