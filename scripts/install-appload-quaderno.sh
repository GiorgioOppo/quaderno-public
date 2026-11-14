#!/bin/sh
set -eu

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Esegui sul Paper Pro con XOVI e AppLoad già installati.

  sh install-appload-quaderno.sh /home/root/quaderno/installation-XXXXXX
  sh install-appload-quaderno.sh --remove

La rimozione toglie il collegamento dal menu AppLoad e conserva Quaderno
con i suoi quaderni. In AppLoad, premi Reload per aggiornare il menu.
EOF
}

[ "$#" -eq 1 ] || { usage >&2; exit 2; }
case "$1" in
    -h|--help) usage; exit 0 ;;
esac

[ "$(id -u)" = 0 ] && [ "$(uname -m)" = aarch64 ] ||
    die "Esegui lo script come root sul reMarkable Paper Pro."
model=$(cat /sys/devices/soc0/machine 2>/dev/null || true)
case "$model" in
    *Ferrari*|*ferrari*) ;;
    *)
        if [ -r /proc/device-tree/model ]; then
            model=$(tr -d '\000' < /proc/device-tree/model)
        fi
        case "$model" in
            *Ferrari*|*ferrari*|'reMarkable Paper Pro') ;;
            *) die "Il dispositivo collegato deve essere un reMarkable Paper Pro." ;;
        esac
        ;;
esac

base=/home/root/quaderno
xovi=/home/root/xovi
appload="$xovi/exthome/appload"
entry="$appload/quaderno"
current="$base/current"
owner=managed-by-quaderno
owner_text=quaderno-appload-v1

if [ "$1" = --remove ] && [ ! -e "$base" ] && [ ! -L "$base" ] &&
    [ ! -e "$entry" ] && [ ! -L "$entry" ]; then
    printf '%s\n' "La voce Quaderno è già assente."
    exit 0
fi

[ -d "$base" ] && [ "$(CDPATH='' cd -P "$base" && pwd)" = "$base" ] ||
    die "Impossibile trovare la cartella delle installazioni di Quaderno."
if [ -e "$appload" ] || [ -L "$appload" ]; then
    [ -d "$appload" ] && [ "$(CDPATH='' cd -P "$appload" && pwd)" = "$appload" ] ||
        die "La cartella AppLoad usa un percorso non previsto."
fi

# The directory descriptor locks updates and removals without leaving a lock file.
command -v flock >/dev/null || die "flock non è disponibile sul dispositivo."
exec 9<"$base"
flock -n 9 || die "Un’altra modifica della voce Quaderno è già in corso."

owns_entry=false
if [ -e "$entry" ] || [ -L "$entry" ]; then
    [ -d "$entry" ] && [ ! -L "$entry" ] && [ -f "$entry/$owner" ] &&
        [ ! -L "$entry/$owner" ] && [ "$(cat "$entry/$owner")" = "$owner_text" ] ||
        die "Esiste già una voce AppLoad Quaderno gestita da un’altra installazione."
    for member in "$entry"/* "$entry"/.[!.]* "$entry"/..?*; do
        if [ ! -e "$member" ] && [ ! -L "$member" ]; then
            continue
        fi
        case "$member" in
            "$entry/$owner"|"$entry/launch.sh"|"$entry/icon.png"|"$entry/external.manifest.json")
                [ -f "$member" ] && [ ! -L "$member" ] ||
                    die "La voce AppLoad Quaderno contiene file con un formato imprevisto."
                ;;
            *) die "La voce AppLoad Quaderno contiene file che appartengono a un’altra installazione." ;;
        esac
    done
    owns_entry=true
fi

valid_release_name() {
    case "$1" in
        installation-?*)
            case "${1#installation-}" in *[!a-zA-Z0-9]*) return 1 ;; esac
            ;;
        *) return 1 ;;
    esac
}

if [ -e "$current" ] || [ -L "$current" ]; then
    if [ ! -L "$current" ] || ! valid_release_name "$(readlink "$current")"; then
        die "Il percorso current appartiene a un’altra installazione."
    fi
fi

if [ "$1" = --remove ]; then
    if [ "$owns_entry" = true ]; then
        staging=$(mktemp -d "$xovi/exthome/.quaderno.XXXXXX")
        trap 'rm -rf "$staging"' 0
        trap 'exit 1' HUP INT TERM
        mv -T "$entry" "$staging/entry"
        [ ! -L "$current" ] || rm "$current"
        printf '%s\n' "Voce Quaderno rimossa dal menu AppLoad."
    else
        printf '%s\n' "La voce Quaderno è già assente."
    fi
    exit 0
fi

[ -d "$appload" ] && [ -r "$xovi/extensions.d/appload.so" ] ||
    die "Installa e verifica XOVI e AppLoad prima di registrare Quaderno."
release="$1"
release_name=${release##*/}
[ "$release" = "$base/$release_name" ] && valid_release_name "$release_name" &&
    [ -d "$release" ] && [ ! -L "$release" ] &&
    [ -f "$release/try-on-device.sh" ] && [ ! -L "$release/try-on-device.sh" ] &&
    [ -f "$release/quaderno" ] && [ ! -L "$release/quaderno" ] ||
    die "Indica un’installazione Quaderno in /home/root/quaderno/installation-XXXXXX."
sh "$release/try-on-device.sh" --check ||
    die "La verifica dell’installazione Quaderno non è riuscita."

bundle=$(CDPATH='' cd -P "$(dirname "$0")" && pwd) ||
    die "Impossibile trovare i file della voce AppLoad."
launcher="$bundle/appload-quaderno-entry.sh"
icon="$bundle/quaderno-appload.png"
if [ ! -f "$launcher" ] || [ ! -f "$icon" ] || ! sh -n "$launcher"; then
    die "Estrai il launcher e l’icona accanto a questo script."
fi
png_signature=$(dd if="$icon" bs=1 count=8 2>/dev/null | od -b |
    awk 'NR == 1 { for (i = 2; i <= NF; i++) printf "%s", $i }')
[ "$png_signature" = 211120116107015012032012 ] || die "L’icona deve essere un file PNG."

umask 077
staging=$(mktemp -d "$xovi/exthome/.quaderno.XXXXXX")
pin_stage=
cleanup() {
    rm -rf "$staging"
    [ -z "$pin_stage" ] || rm -rf "$pin_stage"
}
trap cleanup 0
trap 'exit 1' HUP INT TERM
pin_stage=$(mktemp -d "$base/.current.XXXXXX")
ln -s "$release_name" "$pin_stage/current"

mkdir "$staging/entry"
cp "$launcher" "$staging/entry/launch.sh"
chmod u+x "$staging/entry/launch.sh"
cp "$icon" "$staging/entry/icon.png"
printf '%s\n' "$owner_text" > "$staging/entry/$owner"
cat > "$staging/entry/external.manifest.json" <<'EOF'
{
  "name": "Quaderno",
  "application": "/home/root/xovi/exthome/appload/quaderno/launch.sh",
  "workingDirectory": "/home/root/quaderno",
  "qtfb": true,
  "disablesWindowedMode": true,
  "aspectRatio": "original"
}
EOF

mv -fT "$pin_stage/current" "$current"
if [ "$owns_entry" = true ]; then
    mv -fT "$staging/entry/launch.sh" "$entry/launch.sh"
    mv -fT "$staging/entry/icon.png" "$entry/icon.png"
    mv -fT "$staging/entry/external.manifest.json" "$entry/external.manifest.json"
else
    mv -nT "$staging/entry" "$entry"
    [ ! -e "$staging/entry" ] || die "La voce Quaderno è stata creata da un’altra installazione."
fi

printf 'Quaderno registrato in AppLoad. Installazione selezionata: %s\n' "$release"
printf '%s\n' 'In AppLoad, premi Reload per aggiornare il menu.'
