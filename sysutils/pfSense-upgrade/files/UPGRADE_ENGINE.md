# Kontrol-upgrade v2 Engine

## Sequência de fases

```mermaid
sequenceDiagram
    participant CLI as Kontrol-upgrade CLI
    participant ST as state.json
    participant PKG as pkg-static (isolated/main)
    participant RC as rc.d/kontrol_upgrade

    CLI->>ST: DETECT transition (repo+ABI resolved)
    CLI->>PKG: isolated update/upgrade -n
    CLI->>ST: PREPARE transition + manifest
    CLI->>PKG: fetch artifacts (kernel/base/core/meta/pkg)
    CLI->>ST: STAGE1_APPLY transition
    CLI->>PKG: apply kernel/base
    CLI->>ST: REBOOT_PENDING + expected boot token
    CLI->>RC: reboot scheduled
    RC->>CLI: resume --boot-token <current>
    CLI->>PKG: apply core/meta
    CLI->>ST: STAGE2_APPLY
    CLI->>PKG: integrity checks
    CLI->>ST: STAGE3_FINALIZE -> DONE
```

## Plano de migração do legado

1. **Instalação paralela**: substituir apenas `/usr/local/libexec/Kontrol-upgrade`, mantendo wrapper e comando público.
2. **Compatibilidade de CLI**: flags legadas `-c -u -n -R -y -b` continuam aceitas e mapeadas para subcomandos.
3. **Ativação pós-boot**: habilitar rc script `kontrol_upgrade` para executar `resume` somente quando o estado explícito for `REBOOT_PENDING`.
4. **Rollback operacional**:
   - Se subcomando falhar, estado muda para `FAILED` com último erro.
   - Operador pode executar `abort` para marcar `ROLLBACK_NEEDED`.
   - Rotas de recuperação devem consultar `state.json` e `manifest.json` para reaplicar versão anterior.
5. **Cutover**:
   - validar `check` e `prepare` em ambiente de staging;
   - depois validar cenário com reboot real;
   - somente então remover artefatos legados antigos.
