# Integração dos repositórios do Kontrol no build

O port não é a fonte final dos arquivos de repositório nas imagens do Kontrol.
Durante `poudriere_bulk`, `tools/builder_common.sh` copia todos os arquivos de
`tools/templates/pkg_repos` por cima de `sysutils/Kontrol-repo/files`. Portanto,
uma correção feita somente neste port pode ser sobrescrita antes da compilação.

## RELENG_2_8_1 do kontrol-br/pfsense

O build do Kontrol 2.8.1 deve exportar as três branches abaixo em
`tools/builder_defaults.sh`:

```sh
PKG_REPO_BRANCH_RELEASE="v2_8_1"
PKG_REPO_BRANCH_PREVIOUS="v2_7_2"
PKG_REPO_BRANCH_UPGRADE="v2_9_0"
export PKG_REPO_BRANCH_RELEASE PKG_REPO_BRANCH_PREVIOUS
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
| `Kontrol-repo.conf` | `v2_8_1`, FreeBSD 15 | Atual e padrão |
| `Kontrol-repo-previous.conf` | `v2_7_2`, FreeBSD 14 | Retorno à release anterior |
| `Kontrol-repo-upgrade.conf` | `v2_9_0`, FreeBSD 16 | Detecção e upgrade para 2.9.0 |

O arquivo `Kontrol-repo.conf.default` precisa continuar associado ao
`Kontrol-repo.conf` de 2.8.1. O template principal não pode apontar para 2.9.0:
isso troca o repositório ativo quando o pacote `Kontrol-repo` é atualizado e
expõe o FreeBSD 15 ao `pkg` compilado para FreeBSD 16.

Os templates devem usar `%%PKG_REPO_BRANCH_RELEASE%%`,
`%%PKG_REPO_BRANCH_PREVIOUS%%` e `%%PKG_REPO_BRANCH_UPGRADE%%`, em vez de
versões literais. A função que renderiza os templates, `setup_pkg_repo`, deve
substituir as três variáveis. O `poudriere_bulk` deve copiar os mesmos templates
para o port renomeado, sem trocar qual arquivo possui o marcador `.default`.

`tools/conf/pfPorts/make.conf` também deve incluir o novo repositório no valor
usado para gerar o conjunto do pacote:

```make
PFSENSE_PKG_SET_VERSION=	Kontrol-repo Kontrol-repo-previous \
				Kontrol-repo-upgrade
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
