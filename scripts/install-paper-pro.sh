#!/bin/sh
set -eu

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

mode=demo
case "$#" in
    0) ;;
    1)
        case "$1" in
            --install-only) mode=install ;;
            --appload) mode=appload ;;
            -h|--help)
                cat <<'EOF'
Collega il reMarkable Paper Pro al Mac con un cavo USB‑C per dati.
Estrai quaderno, try-on-device.sh e questo script nella stessa cartella.
Se il pacchetto include il riconoscimento di testo nei PDF e nelle immagini,
estrai anche il motore OCR, il modello, il dizionario, la libreria e le licenze.

  sh install-paper-pro.sh                 Installa e avvia la prova.
  sh install-paper-pro.sh --install-only  Installa senza avviare la prova.
  sh install-paper-pro.sh --appload       Aggiunge Quaderno al menu AppLoad già installato.

Quando SSH lo chiede, inserisci nel Terminale la password del tablet.
EOF
                exit 0
                ;;
            *) die "Usa --help per vedere i comandi disponibili." ;;
        esac
        ;;
    *) die "Usa --help per vedere i comandi disponibili." ;;
esac

bundle=$(CDPATH='' cd -P "$(dirname "$0")" && pwd) ||
    die "Impossibile trovare la cartella del pacchetto."
set -- quaderno try-on-device.sh
if [ "$mode" = appload ]; then
    set -- "$@" install-appload-quaderno.sh appload-quaderno-entry.sh quaderno-appload.png
fi
handwriting=0
if [ -e "$bundle/quaderno-handwriting" ] || [ -e "$bundle/quaderno-handwriting-it.dct" ]; then
    handwriting=1
    set -- "$@" quaderno-handwriting quaderno-handwriting-it.dct quaderno-handwriting-en.dct
fi
raster_ocr=0
for item in quaderno-raster-ocr quaderno-raster-ocr.onnx quaderno-raster-ocr.txt libonnxruntime.so.1; do
    if [ -e "$bundle/$item" ] || [ -L "$bundle/$item" ]; then
        raster_ocr=1
        break
    fi
done
if [ "$raster_ocr" = 1 ]; then
    set -- "$@" quaderno-raster-ocr quaderno-raster-ocr.onnx quaderno-raster-ocr.txt libonnxruntime.so.1 \
        ONNXRuntime-LICENSE.txt ONNXRuntime-ThirdPartyNotices.txt PaddleOCR-LICENSE.txt
fi
for item do
    [ -f "$bundle/$item" ] && [ ! -L "$bundle/$item" ] ||
        die "Estrai $item accanto a questo script."
done

header=$(od -An -tx1 -N6 "$bundle/quaderno" | tr -d '[:space:]')
machine=$(od -An -tx1 -j18 -N2 "$bundle/quaderno" | tr -d '[:space:]')
[ "$header" = 7f454c460201 ] && [ "$machine" = b700 ] ||
    die "Il pacchetto richiede l’eseguibile Quaderno per Linux AArch64."

scratch=$(mktemp -d "${TMPDIR:-/tmp}/quaderno-install.XXXXXX") ||
    die "Impossibile creare la cartella temporanea."
trap 'rm -rf "$scratch"' 0
trap 'exit 1' HUP INT TERM

archive="$scratch/quaderno.tar"
COPYFILE_DISABLE=1 tar --format=ustar -C "$bundle" -cf "$archive" "$@" ||
    die "Impossibile preparare i file da trasferire."

cat > "$scratch/remote.sh" <<'REMOTE'
set -eu

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

[ "$(id -u)" = 0 ] && [ "$(uname -m)" = aarch64 ] ||
    die "Il dispositivo collegato deve essere un reMarkable Paper Pro."
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

set -- quaderno try-on-device.sh
if [ "$mode" = appload ]; then
    [ -r /home/root/xovi/extensions.d/appload.so ] ||
        die "Il collegamento richiede XOVI e AppLoad già installati."
    [ "$(cd -P /home/root/xovi && pwd -P)" = /home/root/xovi ] ||
        die "La cartella XOVI usa un percorso non previsto."
    mkdir -p /home/root/xovi/exthome ||
        die "Impossibile preparare il menu AppLoad."
    [ "$(cd -P /home/root/xovi/exthome && pwd -P)" = /home/root/xovi/exthome ] ||
        die "La cartella AppLoad usa un percorso non previsto."
    mkdir -p /home/root/xovi/exthome/appload || die "Impossibile creare il menu AppLoad."
    set -- "$@" install-appload-quaderno.sh appload-quaderno-entry.sh quaderno-appload.png
fi
if [ "$handwriting" = 1 ]; then
    set -- "$@" quaderno-handwriting quaderno-handwriting-it.dct quaderno-handwriting-en.dct
fi
if [ "$raster_ocr" = 1 ]; then
    set -- "$@" quaderno-raster-ocr quaderno-raster-ocr.onnx quaderno-raster-ocr.txt libonnxruntime.so.1 \
        ONNXRuntime-LICENSE.txt ONNXRuntime-ThirdPartyNotices.txt PaddleOCR-LICENSE.txt
fi

umask 077
base=/home/root/quaderno
[ ! -L "$base" ] || die "La cartella di installazione è un collegamento simbolico."
mkdir -p "$base" || die "Impossibile creare la cartella di installazione."
stage=$(mktemp -d "$base/.install.XXXXXX") ||
    die "Impossibile creare la cartella temporanea sul Paper Pro."
pointer_stage=
cleanup() {
    if [ -n "$stage" ]; then
        rm -rf "$stage"
    fi
    if [ -n "$pointer_stage" ]; then
        rm -f "$pointer_stage"
    fi
}
trap cleanup 0
trap 'exit 1' HUP INT TERM

tar -xf - -C "$stage" "$@" ||
    die "Trasferimento incompleto."
chmod u+x "$stage/quaderno" || die "Impossibile rendere Quaderno eseguibile."
if [ "$handwriting" = 1 ]; then
    chmod u+x "$stage/quaderno-handwriting" || die "Impossibile rendere eseguibile il motore di trascrizione."
fi
if [ "$raster_ocr" = 1 ]; then
    chmod u+x "$stage/quaderno-raster-ocr" || die "Impossibile rendere eseguibile il motore OCR."
fi
sh "$stage/try-on-device.sh" --check ||
    die "La verifica di compatibilità sul Paper Pro non è riuscita."

release="$base/installation-${stage##*.}"
if [ -e "$release" ] || [ -L "$release" ] || ! mv "$stage" "$release"; then
    die "Impossibile completare l’installazione."
fi
stage=

# Only register installations that support the headless transfer protocol.
if command -v timeout >/dev/null &&
    [ "$(HOME=/home/root XDG_DATA_HOME=/home/root/.local/share QT_QPA_PLATFORM=offscreen \
        timeout 5 "$release/quaderno" --usb-transfer-protocol </dev/null 2>/dev/null)" = 1 ]; then
    pointer_stage=$(mktemp "$base/.usb-transfer.XXXXXX") ||
        die "Impossibile preparare il riferimento per il trasferimento USB."
    printf '%s\n' "$release" > "$pointer_stage" &&
        mv -f -T "$pointer_stage" "$base/usb-transfer-installation" ||
        die "Impossibile registrare l’installazione per il trasferimento USB."
    pointer_stage=
fi

printf 'Quaderno installato in %s\n' "$release"
printf "Per riavviare la prova dal Mac, esegui\n  ssh root@10.11.99.1 'sh %s/try-on-device.sh'\n" "$release"
if [ "$mode" = demo ]; then
    exec sh "$release/try-on-device.sh"
fi
if [ "$mode" = appload ]; then
    sh "$release/install-appload-quaderno.sh" "$release"
    printf "Per rimuovere la voce dal Mac, esegui\n  ssh root@10.11.99.1 'sh %s/install-appload-quaderno.sh --remove'\n" "$release"
fi
REMOTE

remote=$(cat "$scratch/remote.sh")

printf 'Collegamento al Paper Pro via USB‑C. Inserisci la password SSH del tablet se richiesta.\n'
ssh -T -o HostName=10.11.99.1 -o ProxyCommand=none -o ProxyJump=none \
    -o ClearAllForwardings=yes -o ForwardAgent=no -o ForwardX11=no \
    -o PermitLocalCommand=no -o BatchMode=no \
    -o ConnectionAttempts=1 -o ConnectTimeout=8 root@10.11.99.1 \
    "mode=$mode
handwriting=$handwriting
raster_ocr=$raster_ocr
$remote" < "$archive"
