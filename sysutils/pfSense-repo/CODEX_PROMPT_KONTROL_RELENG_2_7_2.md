# Prompt para o Codex — Kontrol `RELENG_2_7_2`

Copie todo o conteúdo do bloco abaixo para uma sessão do Codex aberta no
repositório `kontrol-br/pfsense`, branch `RELENG_2_7_2`.

```text
Você está no repositório https://github.com/kontrol-br/pfsense.git, branch
RELENG_2_7_2. Trabalhe exclusivamente nessa branch. Antes de editar, leia todos
os AGENTS.md aplicáveis e examine o git status. Não altere páginas PHP nem o
comportamento da GUI; este trabalho é somente nos scripts/configurações de build
e nos templates de repositório.

Contexto técnico
================

O Kontrol 2.7.2 executa FreeBSD 14 e possui pkg-1.20.8_3. O destino do upgrade é
Kontrol 2.9.0, baseado em FreeBSD 16, cujo pkg não pode executar no userland do
FreeBSD 14 (por exemplo, falta libutil.so.10). Hoje, os templates desta branch
fazem Kontrol-repo.conf apontar para v2_9_0/FreeBSD 16 e usam o 2.7.2 como
"previous". Isso é perigoso: durante poudriere_bulk(), builder_common.sh copia
tools/templates/pkg_repos/* sobre sysutils/Kontrol-repo/files. Ao atualizar o
pacote Kontrol-repo no appliance 2.7.2, o arquivo padrão é sobrescrito pelo
repositório 2.9.0; em seguida, uma consulta/refresh pode instalar o pkg do
FreeBSD 16 e quebrar todo o processo.

Objetivo
========

Fazer o build do RELENG_2_7_2 produzir três opções independentes:

1. Kontrol-repo: atual e DEFAULT, v2_7_2, ABI FreeBSD 14.
2. Kontrol-repo-previous: retorno para v2_7_0, ABI FreeBSD 14.
3. Kontrol-repo-upgrade: destino de detecção/upgrade v2_9_0, ABI FreeBSD 16.

Atualizar o pacote Kontrol-repo no 2.7.2 nunca pode trocar automaticamente o
repositório selecionado para 2.9.0. O marcador .default deve pertencer ao
Kontrol-repo.conf de v2_7_2. A seleção do repositório de upgrade deve ser uma
ação explícita.

Investigação obrigatória antes da edição
========================================

Leia integralmente, incluindo funções relacionadas e chamadores:

- tools/builder_defaults.sh
- build.sh
- tools/builder_common.sh, principalmente setup_pkg_repo(),
  poudriere_rename_ports() e poudriere_bulk()
- tools/conf/pfPorts/make.conf
- todos os arquivos em tools/templates/pkg_repos/
- no FreeBSD-ports usado pelo build, sysutils/pfSense-repo/Makefile e seus
  templates, para confirmar os placeholders aceitos

Não presuma que mudar FreeBSD-ports basta: demonstre no resumo como o overlay
de templates do builder afeta o pacote final.

Alterações requeridas
=====================

1. Em tools/builder_defaults.sh:
   - manter PKG_REPO_BRANCH_RELEASE="v2_7_2";
   - definir PKG_REPO_BRANCH_PREVIOUS="v2_7_0";
   - adicionar PKG_REPO_BRANCH_UPGRADE="v2_9_0";
   - exportar as três variáveis sem duplicar atribuições desnecessariamente.

2. Em build.sh:
   - adicionar PKG_REPO_BRANCH_UPGRADE à validação de variáveis obrigatórias
     para builds publicados;
   - se PREVIOUS for utilizado durante renderização/publicação, validá-lo
     também.

3. Em tools/builder_common.sh:
   - passar PKG_REPO_BRANCH_PREVIOUS e PKG_REPO_BRANCH_UPGRADE para o make.conf
     do poudriere;
   - adicionar substituições para %%PKG_REPO_BRANCH_PREVIOUS%% e
     %%PKG_REPO_BRANCH_UPGRADE%% em setup_pkg_repo();
   - respeitar staging: documentar/implementar claramente se upgrade e previous
     devem continuar apontando para releases ou para staging;
   - manter o overlay de templates determinístico, sem deixar arquivos antigos
     de uma compilação anterior no port renomeado;
   - não alterar o symlink/default para o template de upgrade.

4. Em tools/conf/pfPorts/make.conf:
   - incluir Kontrol-repo-upgrade no valor de PFSENSE_PKG_SET_VERSION;
   - preservar Kontrol-repo e Kontrol-repo-previous.

5. Em tools/templates/pkg_repos:
   - Kontrol-repo.conf deve usar %%PKG_REPO_BRANCH_RELEASE%% e apontar tanto o
     repositório core quanto o principal para v2_7_2 após renderização;
   - Kontrol-repo.abi deve ser FreeBSD:14:%%ARCH%%;
   - Kontrol-repo.altabi deve usar freebsd:14:%%ARCH%% (prefixo minúsculo);
   - Kontrol-repo.descr deve identificar 2.7.2 como Current/Default;
   - Kontrol-repo-previous.conf deve usar
     %%PKG_REPO_BRANCH_PREVIOUS%% para core e principal;
   - Kontrol-repo-previous ABI/ALTABI devem ser FreeBSD 14;
   - criar Kontrol-repo-upgrade.conf usando
     %%PKG_REPO_BRANCH_UPGRADE%% para core e principal;
   - criar Kontrol-repo-upgrade.abi com FreeBSD:16:%%ARCH%%;
   - criar Kontrol-repo-upgrade.altabi com freebsd:16:%%ARCH%%;
   - criar uma descrição clara: Upgrade to 2.9.0 (FreeBSD 16).

6. Verifique o código que cria o marcador *.default e prove que ele resulta em:
   /usr/local/share/Kontrol/pkg/repos/Kontrol-repo.conf.default
   e não em Kontrol-repo-upgrade.conf.default.

7. Não atualize pkg, não adicione bootstrap e não tente resolver o problema
   alterando PHP. Esta branch apenas deve produzir configurações seguras; o
   bloqueio do pkg durante o cross-major pertence ao port Kontrol-upgrade.

Testes obrigatórios
===================

- Execute sh -n em cada shell script alterado.
- Execute git diff --check.
- Crie um teste temporário ou comando reproduzível que renderize os três
  templates para PRODUCT_NAME=Kontrol e ARCH=amd64.
- Garanta que nenhum placeholder %%...%% permaneça nos arquivos renderizados.
- Valide exatamente as URLs finais esperadas:
  Kontrol_v2_7_2_amd64-core
  Kontrol_v2_7_2_amd64-Kontrol_v2_7_2
  Kontrol_v2_7_0_amd64-core
  Kontrol_v2_7_0_amd64-Kontrol_v2_7_0
  Kontrol_v2_9_0_amd64-core
  Kontrol_v2_9_0_amd64-Kontrol_v2_9_0
- Simule a cópia executada por poudriere_bulk para um diretório limpo e valide
  que os arquivos Kontrol-repo, previous e upgrade existem com .conf, .descr,
  .abi e .altabi.
- Valide que somente Kontrol-repo.conf recebe o marcador .default.
- Se o ambiente permitir, execute o teste/build específico existente do
  projeto. Não alegue ter executado poudriere ou make package sem fazê-lo.

Entrega
=======

- Mostre o diagnóstico da causa raiz.
- Liste cada arquivo alterado e por quê.
- Mostre as URLs e ABIs renderizadas.
- Informe comandos e resultados reais dos testes.
- Aumente a versão/revisão apropriada de qualquer artefato versionado do build
  que precise ser republicado, seguindo a convenção já usada na branch.
- Faça commit das alterações na branch atual e prepare um PR com título e corpo
  explicando que esta é a metade RELENG_2_7_2 da correção cross-major.
```
