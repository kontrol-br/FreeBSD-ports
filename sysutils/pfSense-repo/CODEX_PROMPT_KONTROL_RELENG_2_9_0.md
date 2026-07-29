# Prompt para o Codex — Kontrol `RELENG_2_9_0`

Copie todo o conteúdo do bloco abaixo para uma sessão do Codex aberta no
repositório `kontrol-br/pfsense`, branch `RELENG_2_9_0`.

```text
Você está no repositório https://github.com/kontrol-br/pfsense.git, branch
RELENG_2_9_0. Trabalhe exclusivamente nessa branch. Antes de editar, leia todos
os AGENTS.md aplicáveis e examine o git status. Não altere páginas PHP nem o
comportamento da GUI; este trabalho é somente nos scripts/configurações de build
e nos templates de repositório.

Contexto técnico
================

O Kontrol 2.9.0 executa FreeBSD 16 e é o destino do upgrade iniciado no Kontrol
2.7.2/FreeBSD 14. A branch 2.9.0 já possui templates para 2.9.0, 2.8.1 e 2.7.2,
mas os endereços estão duplicados como literais nos templates e como variáveis
em builder_defaults.sh. Também existem inconsistências de capitalização em
ALTABI (freeBSD versus freebsd). Precisamos tornar os templates e o builder
coerentes com a metade RELENG_2_7_2 da correção, sem mudar o default do 2.9.0.

Objetivo
========

Fazer o RELENG_2_9_0 produzir e preservar estas opções independentes:

1. Kontrol-repo: atual e DEFAULT, v2_9_0, ABI FreeBSD 16.
2. Kontrol-repo-previous: retorno para v2_8_1, ABI FreeBSD 15.
3. Kontrol-repo-devel (ou um nome de compatibilidade claramente documentado):
   retorno legado para v2_7_2, ABI FreeBSD 14.

Não introduza Kontrol-repo-upgrade como default nesta branch: depois que o
sistema já executa 2.9.0, Kontrol-repo é o repositório normal. Preserve os nomes
existentes consumidos pela GUI, a menos que uma análise de todos os chamadores
prove que uma migração de nome é segura e implemente compatibilidade.

Investigação obrigatória antes da edição
========================================

Leia integralmente, incluindo funções relacionadas e chamadores:

- tools/builder_defaults.sh
- build.sh
- tools/builder_common.sh, principalmente setup_pkg_repo(),
  poudriere_rename_ports() e poudriere_bulk()
- tools/conf/pfPorts/make.conf
- todos os arquivos em tools/templates/pkg_repos/
- a implementação de pkg_list_repos(), pkg_get_default_repo() e
  pkg_switch_repo() somente para entender o contrato de nomes; não edite PHP
- no FreeBSD-ports RELENG_2_9_0, sysutils/pfSense-repo e
  sysutils/pfSense-upgrade, especialmente ABI/OSVERSION e pkg 2.x

Compare também com RELENG_2_7_2 para garantir que o repositório upgrade criado
lá termina exatamente no Kontrol-repo padrão desta branch.

Alterações requeridas
=====================

1. Em tools/builder_defaults.sh:
   - manter PKG_REPO_BRANCH_RELEASE="v2_9_0";
   - manter PKG_REPO_BRANCH_PREVIOUS="v2_8_1";
   - definir uma variável explícita para o retorno legado v2_7_2, reutilizando
     PKG_REPO_BRANCH_DEVEL somente se esse for realmente o contrato existente;
   - remover atribuições/exportações duplicadas sem mudar os valores efetivos.

2. Em build.sh:
   - validar todas as variáveis de branch necessárias para builds publicados;
   - não exigir uma variável que não participe do conjunto de templates.

3. Em tools/builder_common.sh:
   - passar RELEASE, PREVIOUS e DEVEL/LEGACY ao make.conf do poudriere;
   - fazer setup_pkg_repo() substituir
     %%PKG_REPO_BRANCH_RELEASE%%,
     %%PKG_REPO_BRANCH_PREVIOUS%% e
     %%PKG_REPO_BRANCH_DEVEL%% (ou %%PKG_REPO_BRANCH_LEGACY%%);
   - manter tratamento de staging explícito e coerente;
   - tornar o overlay de tools/templates/pkg_repos determinístico, removendo
     templates obsoletos antes da cópia quando necessário;
   - preservar Kontrol-repo.conf como default.

4. Em tools/templates/pkg_repos:
   - Kontrol-repo.conf deve usar %%PKG_REPO_BRANCH_RELEASE%% para core e
     principal e renderizar v2_9_0;
   - Kontrol-repo.abi deve ser FreeBSD:16:%%ARCH%%;
   - Kontrol-repo.altabi deve ser freebsd:16:%%ARCH%%;
   - Kontrol-repo-previous.conf deve usar
     %%PKG_REPO_BRANCH_PREVIOUS%% e renderizar v2_8_1;
   - previous ABI/ALTABI devem ser FreeBSD 15;
   - Kontrol-repo-devel.conf (ou legacy) deve usar a variável da branch v2_7_2;
   - devel/legacy ABI/ALTABI devem ser FreeBSD 14;
   - todos os prefixos ALTABI devem ser exatamente freebsd em minúsculas;
   - manter descrições inequívocas: Current 2.9.0, Previous 2.8.1 e Legacy
     2.7.2. Não chame 2.7.2 de development se ele é um repositório de retorno;
     se o nome de arquivo precisar ficar por compatibilidade, explique isso na
     descrição.

5. Em tools/conf/pfPorts/make.conf:
   - garantir que PFSENSE_PKG_SET_VERSION contenha exatamente os templates
     distribuídos por esta branch: Kontrol-repo, Kontrol-repo-previous e
     Kontrol-repo-devel/legacy;
   - não incluir o Kontrol-repo-upgrade da branch 2.7.2 se ele não for instalado
     pelo pacote 2.9.0.

6. Verifique que o pacote Kontrol-repo 2.9.0 não sobrescreve a escolha salva do
usuário, exceto quando o caminho salvo deixou de existir. O marcador .default
deve continuar em Kontrol-repo.conf.

7. Não altere a lógica PHP, não implemente downgrade automático e não tente
executar pkg de FreeBSD 16 no FreeBSD 14. Esta tarefa somente torna a saída do
build 2.9.0 coerente e compatível com o ponto de chegada do upgrade.

Testes obrigatórios
===================

- Execute sh -n em cada shell script alterado.
- Execute git diff --check.
- Renderize todos os templates para PRODUCT_NAME=Kontrol e ARCH=amd64.
- Garanta que nenhum placeholder %%...%% permaneça.
- Valide exatamente as URLs finais esperadas:
  Kontrol_v2_9_0_amd64-core
  Kontrol_v2_9_0_amd64-Kontrol_v2_9_0
  Kontrol_v2_8_1_amd64-core
  Kontrol_v2_8_1_amd64-Kontrol_v2_8_1
  Kontrol_v2_7_2_amd64-core
  Kontrol_v2_7_2_amd64-Kontrol_v2_7_2
- Valide ABI/ALTABI finais para FreeBSD 16, 15 e 14.
- Simule o overlay de poudriere_bulk em diretório limpo e valide que não há
  templates herdados/obsoletos.
- Valide que somente Kontrol-repo.conf possui o marcador .default.
- Compare a URL do Kontrol-repo-upgrade renderizado no RELENG_2_7_2 com a URL
  do Kontrol-repo padrão desta branch; elas devem ser idênticas.
- Se o ambiente permitir, execute testes/builds específicos reais. Não alegue
  ter executado poudriere ou make package sem fazê-lo.

Entrega
=======

- Mostre o diagnóstico e o contrato final dos três repositórios.
- Liste cada arquivo alterado e por quê.
- Mostre URLs, ABI e ALTABI renderizados.
- Informe comandos e resultados reais dos testes.
- Aumente versão/revisão de qualquer artefato versionado que precise ser
  republicado, conforme a convenção desta branch.
- Faça commit na branch atual e prepare um PR identificado como a metade
  RELENG_2_9_0 da correção cross-major.
```
