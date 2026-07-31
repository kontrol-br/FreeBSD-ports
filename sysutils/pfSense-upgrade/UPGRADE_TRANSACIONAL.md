# Upgrade transacional do Kontrol/pfSense

## A) Arquitetura

Componentes:

1. **Orquestrador** (`/usr/local/sbin/Kontrol-upgrade`): controla pipeline stage0..stage4, lock global, journal e progresso JSON.
2. **Estado persistente** (`/var/db/Kontrol/upgrade/state`): chaves simples `k=v` com checksum armazenado.
3. **Journal** (`/var/db/Kontrol/upgrade/journal.log`): trilha idempotente por `txid`.
4. **Cache transacional** (`/var/cache/Kontrol-upgrade/<txid>`): manifestos e artefatos da transação.
5. **Serviço de boot** (`/usr/local/etc/rc.d/kontrol-upgrade-stage`): detecta stage pendente no boot e continua com timeout.
6. **Biblioteca PHP** (`kontrol_upgrade_status.inc`): leitura do estado para GUI/telemetria.

## B) Diagrama de estados

```text
idle
  |
  +--> stage0 (discover)
  |
  +--> stage1 (prepare/download/validate) --reboot--> stage2 (apply core)
          |                                      |
          +--------------------error-------------+
                              rollback_needed

stage2 --> stage3 (post-validate) --> stage4 (cleanup/finalize) --> done
   |                |
   +------error-----+------------------> rollback_needed
```

## C) Estrutura de diretórios/arquivos

- `sysutils/pfSense-upgrade/files/Kontrol-upgrade`
- `sysutils/pfSense-upgrade/files/Kontrol-upgrade.wrapper`
- `sysutils/pfSense-upgrade/files/kontrol-upgrade-stage.rc`
- `sysutils/pfSense-upgrade/files/kontrol_upgrade_status.inc`
- `sysutils/pfSense-upgrade/UPGRADE_TRANSACIONAL.md`

## D) Pseudocódigo de cada estágio

- **stage0**: atualizar metadados em memória -> `pkg upgrade -nq` -> exit 2 se upgrade disponível.
- **stage1**: validar assinatura/repo -> detectar ABI atual/alvo -> habilitar `IGNORE_OSVERSION` somente se cross-major -> baixar (`upgrade -F`, `fetch pkg`) -> gerar manifesto -> marcar `stage2`.
- **stage2**: (boot) aplicar `pkg` (quando cross-major) -> `pkg upgrade -fy` -> `pkg check` -> marcar `stage3`.
- **stage3**: validar consistência (`pkg check`) + serviços críticos -> marcar `stage4`.
- **stage4**: `autoremove`/`clean` -> consolidar logs -> marcar `done`.

## E) Implementação inicial funcional

Implementada em shell + rc + helper PHP no port `pfSense-upgrade`.

## F) Checklist de testes e critérios de aceite

1. `sh -n` em scripts shell sem erro.
2. `php -l` na lib de status sem erro.
3. `Kontrol-upgrade -c` retorna `0` (sem upgrade) ou `2` (upgrade disponível).
4. `Kontrol-upgrade -s stage1` cria `txid`, manifesto e marca `stage2`.
5. Serviço rc continua stage pendente em boot quando estado contém `stage2/3/4`.
6. Logs texto e JSON atualizam por estágio.
7. Em falha de estágio, estado deve virar `rollback_needed`.

## Compatibilidade com chamadas do pfSense/Kontrol

O script mantém os nomes e flags usados pelo código atual do pfSense/Kontrol:

- nomes instalados: `Kontrol-upgrade` e `pfSense-upgrade` (symlink de compatibilidade);
- flags legadas aceitas: `-c`, `-u`, `-U`, `-f`, `-i`, `-r`, `-b`, `-l`, `-p`, `-R`, `-n`, `-y`, `-4`, `-6`, `-d`;
- `-b` continua compatível com uso sem argumento e aceita estágio como próximo argumento (`-b stage2`) para orquestração de boot;
- chamadas de metadados usadas no pfSense (`-uf`, `-Uc`) continuam válidas e são interpretadas corretamente.
