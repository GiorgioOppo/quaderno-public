#!/bin/sh
set -eu
umask 077

base=/home/root/quaderno/obsidian-sync
vault="$base/vault"
identity="$base/vault-id"
runtime="$base/runtime-24.22.3-0.0.14"
node="$runtime/bin/node"
cli="$runtime/obsidian-headless/cli.js"

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

valid_id() {
    case "$1" in
        ''|*[!a-zA-Z0-9_-]*) return 1 ;;
    esac
    [ "${#1}" -ge 8 ] && [ "${#1}" -le 128 ]
}

ob() {
    "$node" "$cli" "$@"
}

configured_vault() {
    if config=$(ob sync-status --path "$vault" --json 2>/dev/null); then
        printf '%s' "$config" | "$node" -e '
            const fs = require("node:fs");
            const status = JSON.parse(fs.readFileSync(0, "utf8"));
            if (status.vaultPath !== process.argv[1] || typeof status.vaultId !== "string") process.exit(1);
            if (process.argv[2] === "verify" && (status.syncMode !== "bidirectional"
                || status.conflictStrategy !== "conflict" || status.configs?.length !== 0)) process.exit(1);
            process.stdout.write(status.vaultId);
        ' "$vault" "${1:-}"
    else
        return "$?"
    fi
}

configure_mode() {
    ob sync-config --path "$vault" --mode bidirectional --conflict-strategy conflict --configs '' --json >/dev/null
}

[ -d "$base" ] && [ "$(CDPATH='' cd -P "$base" && pwd)" = "$base" ] ||
    die 'Obsidian Sync non è installato sul Paper Pro.'
[ -x "$node" ] && [ -r "$cli" ] || die 'Il client Obsidian Sync non è disponibile.'
command -v flock >/dev/null || die 'Il dispositivo non dispone di flock.'
exec 9<"$base"
flock -n 9 || die "Un’altra operazione Obsidian Sync è già in corso."

XDG_CONFIG_HOME="$base/config"
export XDG_CONFIG_HOME
unset OBSIDIAN_AUTH_TOKEN NODE_OPTIONS NODE_PATH
for dir in "$XDG_CONFIG_HOME" "$vault"; do
    [ ! -L "$dir" ] || die 'Percorso di Obsidian Sync non valido.'
    mkdir -p "$dir" || die 'Impossibile preparare la cartella di Obsidian Sync.'
    chmod 700 "$dir"
done

if [ -e "$identity" ] || [ -L "$identity" ]; then
    [ -f "$identity" ] && [ ! -L "$identity" ] || die 'Identificativo del vault non valido.'
    vault_id=$(cat "$identity")
    valid_id "$vault_id" || die 'Identificativo del vault non valido.'
else
    vault_id=''
fi

case "${1:-}" in
    setup)
        [ "$#" -le 2 ] && [ -t 0 ] && [ -t 1 ] ||
            die 'Apri un Terminale per inserire direttamente le credenziali Obsidian.'
        if [ -n "${2:-}" ]; then
            valid_id "$2" || die "Usa l’identificativo del vault mostrato da Obsidian."
            [ -z "$vault_id" ] || [ "$2" = "$vault_id" ] ||
                die 'Il dispositivo è già associato a un altro vault Obsidian.'
        fi

        printf '%s\n' 'Accedi a Obsidian Sync. Le note modificate su Quaderno vengono inviate quando confermi un invio.'
        ob login
        if [ -n "$vault_id" ]; then
            [ "$(configured_vault)" = "$vault_id" ] || die 'Il vault locale non corrisponde al vault associato.'
            configure_mode
            printf '%s\n' 'Il vault è già configurato. Apri Obsidian Sync in Quaderno.'
            exit 0
        fi

        if existing_id=$(configured_vault); then
            valid_id "$existing_id" || die 'Identificativo del vault non valido.'
            [ -z "${2:-}" ] || [ "$2" = "$existing_id" ] ||
                die 'La configurazione locale appartiene a un altro vault Obsidian.'
            selected_id=$existing_id
        else
            [ "$?" -eq 3 ] || die 'Impossibile leggere la configurazione Obsidian Sync.'
            for entry in "$vault"/* "$vault"/.[!.]* "$vault"/..?*; do
                if [ -e "$entry" ] || [ -L "$entry" ]; then
                    die 'La cartella locale contiene già file. Usa una copia locale vuota per il primo collegamento.'
                fi
            done
            selected_id=${2:-}
            if [ -z "$selected_id" ]; then
                ob sync-list-remote
                printf '%s' "Incolla l’identificativo del vault Obsidian: "
                IFS= read -r selected_id || die 'Nessun identificativo selezionato.'
            fi
            valid_id "$selected_id" || die "Usa l’identificativo del vault mostrato da Obsidian."
            ob sync-setup --vault "$selected_id" --path "$vault" --device-name 'Quaderno Paper Pro'
            [ "$(configured_vault)" = "$selected_id" ] || die "Il vault configurato non corrisponde all’identificativo selezionato."
        fi
        configure_mode
        temporary_id=$(mktemp "$base/.vault-id.XXXXXX")
        trap 'rm -f "$temporary_id"' 0
        printf '%s\n' "$selected_id" > "$temporary_id"
        mv -f -T "$temporary_id" "$identity"
        printf '%s\n' 'Vault collegato. Apri Obsidian Sync in Quaderno per avviare il primo download.'
        ;;
    sync)
        [ "$#" -eq 1 ] || die 'Comando Obsidian Sync non valido.'
        [ -n "$vault_id" ] && [ "$(configured_vault verify)" = "$vault_id" ] ||
            die 'Configura nuovamente Obsidian Sync sul Paper Pro.'
        exec "$node" "$cli" sync --path "$vault"
        ;;
    *) die 'Usa setup o sync.' ;;
esac
