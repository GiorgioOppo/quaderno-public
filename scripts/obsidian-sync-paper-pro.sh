#!/bin/sh
set -eu
umask 077

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Collega il Paper Pro via USB‑C e abilita il Wi‑Fi sul tablet.

  sh obsidian-sync-paper-pro.sh install          Scarica e installa il client sul Paper Pro.
  sh obsidian-sync-paper-pro.sh setup [vault-id]  Accedi e collega un vault esistente.

Per setup usa un Terminale. Obsidian chiede le credenziali direttamente sul tablet.
È necessario un abbonamento Obsidian Sync attivo.
EOF
}

tablet_ssh() {
    ssh -o HostName=10.11.99.1 -o ProxyCommand=none -o ProxyJump=none \
        -o ClearAllForwardings=yes -o ForwardAgent=no -o ForwardX11=no \
        -o PermitLocalCommand=no -o BatchMode=no -o ConnectionAttempts=1 \
        -o ConnectTimeout=8 "$@"
}

case "${1:-}" in
    setup)
        [ "$#" -le 2 ] && [ -t 0 ] && [ -t 1 ] || die 'Apri un Terminale per configurare Obsidian Sync.'
        set -- "${2:-}"
        case "$1" in
            *[!a-zA-Z0-9_-]*) die "Usa l’identificativo del vault mostrato da Obsidian." ;;
        esac
        printf '%s\n' 'Collegamento al Paper Pro. Inserisci la password SSH del tablet se richiesta.'
        tablet_ssh -tt root@10.11.99.1 \
            "/home/root/quaderno/obsidian-sync/ob-quaderno setup${1:+ $1}"
        exit 0
        ;;
    -h|--help) usage; exit 0 ;;
    install) [ "$#" -eq 1 ] || die 'Usa --help per vedere i comandi disponibili.' ;;
    *) usage >&2; exit 2 ;;
esac

source_dir=$(CDPATH='' cd -P "$(dirname "$0")" && pwd)
wrapper="$source_dir/obsidian-sync-device.sh"
[ -f "$wrapper" ] && [ ! -L "$wrapper" ] || die 'Estrai obsidian-sync-device.sh accanto a questo script.'
for command in curl tar shasum ssh; do
    command -v "$command" >/dev/null || die "Comando mancante: $command"
done

scratch=$(mktemp -d "${TMPDIR:-/tmp}/quaderno-obsidian-sync.XXXXXX")
trap 'rm -rf "$scratch"' 0
trap 'exit 1' HUP INT TERM
node_version=24.22.3
headless_version=0.0.14
sqlite_version=12.11.1
runtime_name="runtime-$node_version-$headless_version"
runtime="$scratch/$runtime_name"
cli="$runtime/obsidian-headless"
mkdir -p "$runtime/bin" "$cli/node_modules"

download() {
    curl --disable --fail --silent --show-error --location --retry 2 \
        --proto '=https' --proto-redir '=https' --connect-timeout 15 --max-time 120 \
        --output "$scratch/$2" "$1" || die "Download non riuscito: $2"
    (cd "$scratch" && printf '%s  %s\n' "$3" "$2" | shasum -a 256 -c - >/dev/null) ||
        die "Impronta SHA‑256 inattesa: $2"
}

npm_package() {
    [ -d "$4" ] || mkdir -p "$4"
    download "https://registry.npmjs.org/$1/-/$1-$2.tgz" "$1.tgz" "$3"
    tar -xzf "$scratch/$1.tgz" --strip-components 1 -C "$4"
}

printf '%s\n' 'Scarico Node.js e il client ufficiale Obsidian Headless per Linux AArch64.'
node_archive="node-v$node_version-linux-arm64.tar.xz"
download "https://nodejs.org/dist/v$node_version/$node_archive" "$node_archive" \
    6e51a2ada00f0011cc266f65d98922b403d7ad88f809ca2aa615a5fe1ce6e86c
tar -xJf "$scratch/$node_archive" --strip-components 2 -C "$runtime/bin" \
    "${node_archive%.tar.xz}/bin/node"
npm_package obsidian-headless "$headless_version" \
    ef75298ed3a356dcb2a4de98c6efe10b2ac64b00556007118b6ac70535e73156 "$cli"
npm_package commander 14.0.3 \
    5849703c500e0f324eb01340d8bda1f9ec48fc37bb7be9712eb0dd52aad92f6c "$cli/node_modules/commander"
npm_package better-sqlite3 "$sqlite_version" \
    ebf0ed75a7a59dbcb3b24bbd014ef49d9f15bc328e4adcbf516f2a8fadfa2835 "$cli/node_modules/better-sqlite3"
npm_package bindings 1.5.0 \
    d77781178c5bd89a91b1f6c5556acd511b1b5927eb13e2ad8189cac29eeb0907 "$cli/node_modules/bindings"
npm_package file-uri-to-path 1.0.0 \
    5440cdf67e75ab96f36a6be63c1d4c3d54255b1d0970273710fecfebfab06fb3 "$cli/node_modules/file-uri-to-path"
sqlite_archive="better-sqlite3-v$sqlite_version-node-v137-linux-arm64.tar.gz"
download "https://github.com/WiseLibs/better-sqlite3/releases/download/v$sqlite_version/$sqlite_archive" \
    "$sqlite_archive" 60da4cbe1b1714c8db62f2ee71a9928cde5303dab57bde14c02debd12a784439
tar -xzf "$scratch/$sqlite_archive" -C "$cli/node_modules/better-sqlite3"

cp "$wrapper" "$scratch/ob-quaderno"
chmod 700 "$scratch/ob-quaderno"
COPYFILE_DISABLE=1 tar --format=ustar -C "$scratch" -czf "$scratch/runtime.tar.gz" "$runtime_name" ob-quaderno

cat > "$scratch/install-on-device.sh" <<'REMOTE'
set -eu
umask 077
die() {
    printf '%s\n' "$*" >&2
    exit 1
}

[ "$(id -u)" = 0 ] && [ "$(uname -m)" = aarch64 ] || die 'Collega un reMarkable Paper Pro.'
model=$(cat /sys/devices/soc0/machine 2>/dev/null || true)
case "$model" in
    *Ferrari*|*ferrari*|'reMarkable Paper Pro') ;;
    *)
        model=$(tr -d '\000' < /proc/device-tree/model 2>/dev/null || true)
        case "$model" in
            *Ferrari*|*ferrari*|'reMarkable Paper Pro') ;;
            *) die 'Collega un reMarkable Paper Pro.' ;;
        esac
        ;;
esac
grep -Eq '^IMG_VERSION="?3\.29\.1\.0"?$' /etc/os-release || die 'Questa build richiede il firmware 3.29.1.0.'
command -v flock >/dev/null || die 'Il dispositivo non dispone di flock.'

quaderno=/home/root/quaderno
base="$quaderno/obsidian-sync"
for dir in "$quaderno" "$base"; do
    [ ! -L "$dir" ] || die 'Percorso di installazione non valido.'
    mkdir -p "$dir"
    [ "$(CDPATH='' cd -P "$dir" && pwd)" = "$dir" ] || die 'Percorso di installazione non valido.'
done
chmod 700 "$base"
exec 9<"$base"
flock -n 9 || die 'Un’altra operazione Obsidian Sync è già in corso.'
stage=$(mktemp -d "$base/.install.XXXXXX")
trap 'rm -rf "$stage"' 0
trap 'exit 1' HUP INT TERM
tar -xzf - -C "$stage"

runtime="$base/$runtime_name"
if [ ! -e "$runtime" ] && [ ! -L "$runtime" ]; then
    runtime="$stage/$runtime_name"
fi
[ -d "$runtime" ] && [ ! -L "$runtime" ] || die 'Percorso del runtime non valido.'
node="$runtime/bin/node"
cli="$runtime/obsidian-headless/cli.js"
unset OBSIDIAN_AUTH_TOKEN NODE_OPTIONS NODE_PATH
export XDG_CONFIG_HOME="$stage/test-config"
[ "$("$node" --version)" = "v$node_version" ] && [ "$("$node" "$cli" --version)" = "$headless_version" ] ||
    die 'Il runtime Obsidian Sync non funziona su questo firmware.'
"$node" -e 'const Database = require(process.argv[1]); new Database(":memory:").close();' \
    "$runtime/obsidian-headless/node_modules/better-sqlite3" || die 'Il modulo SQLite non funziona sul Paper Pro.'
if [ "$runtime" = "$stage/$runtime_name" ]; then
    mv -T "$runtime" "$base/$runtime_name"
fi
mv -f -T "$stage/ob-quaderno" "$base/ob-quaderno"
printf '%s\n' 'Obsidian Headless è installato sul Paper Pro. Esegui ora lo script con setup.'
REMOTE
remote=$(cat "$scratch/install-on-device.sh")

printf '%s\n' 'Collegamento al Paper Pro. Inserisci la password SSH del tablet se richiesta.'
tablet_ssh -T root@10.11.99.1 \
    "runtime_name=$runtime_name node_version=$node_version headless_version=$headless_version
$remote" < "$scratch/runtime.tar.gz"
