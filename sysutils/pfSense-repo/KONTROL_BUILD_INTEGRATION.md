# Integração dos repositórios do Kontrol no build

O port não é a fonte final dos arquivos de repositório nas imagens do Kontrol.
Durante `poudriere_bulk`, `tools/builder_common.sh` copia todos os arquivos de
`tools/templates/pkg_repos` por cima de `sysutils/Kontrol-repo/files`. Portanto,
uma correção feita somente neste port pode ser sobrescrita antes da compilação.

## RELENG_2_8_1 do kontrol-br/pfsense

O `RELENG_2_8_1` atualmente publica dois templates: `Kontrol-repo` é o destino
2.9.0 selecionado na tela de upgrade e `Kontrol-repo-previous` é o sistema
2.8.1 em execução. O port deriva o marcador padrão do sufixo `-previous`, sem
depender de um `PFSENSE_DEFAULT_REPO` herdado do build 2.9.

O build deve exportar as branches abaixo em `tools/builder_defaults.sh`:

```sh
PKG_REPO_BRANCH_UPGRADE="v2_9_0"
export PKG_REPO_BRANCH_UPGRADE
```

`build.sh` deve adicionar `PKG_REPO_BRANCH_UPGRADE` à lista `_required` dos
builds publicados. `tools/builder_common.sh` deve gravar também a variável nova
no make.conf do poudriere:

```make
PKG_REPO_BRANCH_UPGRADE=${PKG_REPO_BRANCH_UPGRADE}
```

Os templates canônicos de `tools/templates/pkg_repos` devem ser:

| Arquivo | Branch/ABI | Uso |
| --- | --- | --- |
| `Kontrol-repo-previous.conf` | `v2_8_1`, FreeBSD 15 | Atual e padrão |
| `Kontrol-repo.conf` | `v2_9_0`, FreeBSD 16 | Detecção e upgrade para 2.9.0 |

O arquivo `Kontrol-repo-previous.conf.default` precisa continuar associado ao
template 2.8.1. `Kontrol-repo.conf` somente se torna ativo depois que o usuário
confirma a branch 2.9.0 na tela de upgrade.

O `poudriere_bulk` deve copiar os mesmos templates para o port renomeado sem
trocar qual arquivo possui o marcador `.default`.

`tools/conf/pfPorts/make.conf` também deve incluir o novo repositório no valor
usado para gerar o conjunto do pacote:

```make
PFSENSE_PKG_SET_VERSION=	Kontrol-repo-previous Kontrol-repo
```

## RELENG_2_9_0 do kontrol-br/pfsense

Na branch 2.9.0, o repositório padrão deve continuar sendo `Kontrol-repo.conf`
com `v2_9_0` e ABI FreeBSD 16. Os repositórios de retorno devem permanecer
separados:

| Arquivo | Branch/ABI |
| --- | --- |
| `Kontrol-repo.conf` | `v2_9_0`, FreeBSD 16 |
| `Kontrol-repo-previous.conf` | `v2_8_1`, FreeBSD 15 |
| `Kontrol-repo-devel.conf` | `v2_7_2`, FreeBSD 14 |

Essa branch já possui os três templates. É necessário apenas garantir que
`PKG_REPO_BRANCH_RELEASE`, `PKG_REPO_BRANCH_PREVIOUS` e a branch de retorno
sejam passadas ao make.conf e usadas nos templates, evitando versões duplicadas
em `builder_defaults.sh` e nos arquivos `.conf`. Os arquivos `.altabi` devem
usar consistentemente o prefixo minúsculo `freebsd`.

## Ordem de publicação

1. Publicar `Kontrol-upgrade` e o `Kontrol-repo` corrigido no repositório 2.8.1.
2. Manter no repositório 2.8.1 o `pkg` compilado para FreeBSD 15.
3. Publicar o repositório 2.9.0 normalmente com o `pkg` de FreeBSD 16.
4. Confirmar que a instalação de `Kontrol-repo` em 2.8.1 mantém o link ativo no
   template 2.8.1.
5. Selecionar `Upgrade to 2.9.0` somente para detecção/execução do upgrade; o
   orquestrador mantém `pkg` bloqueado até o novo kernel/base iniciar.
