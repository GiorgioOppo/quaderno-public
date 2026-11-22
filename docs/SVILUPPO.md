# Sviluppare e compilare Quaderno

[English](en/DEVELOPMENT.md) · **Italiano**

Quaderno è un’applicazione C++17 e Qt Quick. Il progetto genera `quaderno.app` su macOS e un eseguibile Linux AArch64 per reMarkable Paper Pro. Esegui i comandi di questa guida dalla radice del repository, in una shell Bash o Zsh. Per la compilazione incrociata usa Bash su Linux.

- [Installazione e ripristino sul Paper Pro](INSTALLAZIONE.md)
- [Guida all’uso](GUIDA.md)
- [Dati, backup e trasferimento](DATI.md)

## Architettura

| Componente | Responsabilità |
| --- | --- |
| [`qml/`](../qml) e [`src/main.cpp`](../src/main.cpp) | Interfaccia condivisa, navigazione, editor, finestre di dialogo e inizializzazione dei servizi. I file QML sono incorporati nel modulo `Quaderno`. |
| [`Notebook`](../src/notebook.cpp) | Libreria, pagine, livelli, cronologia delle modifiche all’inchiostro, importazione JSON ed esportazione. Usa `QLockFile` per coordinare le scritture e `QSaveFile` per sostituire atomicamente il file dati. |
| [`InkItem`](../src/inkitem.cpp) e [`inkrender`](../src/inkrender.cpp) | Input da mouse, tablet Qt e Marker Linux. I tratti usano coordinate normalizzate. Su Paper Pro la pressione del Marker arriva dagli eventi di `/dev/input`. |
| [`PageContent`](../src/pagecontent.cpp) e [`PagePreviewItem`](../src/pagepreviewitem.cpp) | Rendering di modelli, elementi di pagina, miniature e grafici. Il PDF usa lo stesso codice C++ per disegnare i contenuti di pagina e i tratti. |
| [`shaperecognition`](../src/shaperecognition.cpp) e [`ChartRecognizer`](../src/chartrecognizer.cpp) | Riconoscimento locale delle forme e ricostruzione di grafici da tratti selezionati. |
| [`pdfimport`](../src/pdfimport.cpp) e [`epubimport`](../src/epubimport.cpp) | Importazione locale. Per i PDF usa Core Graphics e PDFKit su macOS e PDFium sul Paper Pro. Legge gli EPUB con libarchive. |
| [`AppleShare`](../src/appleshare.cpp), [`UsbTransfer`](../src/usbtransfer.cpp) e [`Handwriting`](../src/handwriting.cpp) | Condivisione PDF tramite macOS, trasferimento dei quaderni via SSH e trascrizione locale con WritePad su Paper Pro o PencilKit e Vision su macOS. |
| [`AppLanguage`](../src/applanguage.cpp) e [`translations/`](../translations) | Memorizza la lingua dell’interfaccia e carica i cataloghi inglesi incorporati. Le stringhe italiane sono definite nei sorgenti QML e C++. |
| [`WirelessSync`](../src/wirelesssync.cpp), [`SyncChannel`](../src/syncchannel.cpp) e [`BleSync`](../src/blesync.cpp) | Abbina i dispositivi, autentica una sessione TLS 1.3 su Bluetooth LE o TCP locale e gestisce anteprime, aggiornamenti e conflitti dei quaderni. |
| [`BluetoothSettings`](../src/bluetoothsettings.cpp) | Cerca gli accessori sul Paper Pro e gestisce abbinamento, codici e connessioni tramite BlueZ. |
| [`DeviceInfo`](../src/deviceinfo.cpp), [`WifiSettings`](../src/wifisettings.cpp) e [`PinLock`](../src/pinlock.cpp) | Lettura dello stato del tablet, gestione Wi‑Fi via D-Bus e PIN dell’applicazione. |
| [`scripts/`](../scripts) | Installazione via SSH, sessione di prova temporanea con systemd e collegamento opzionale al menu AppLoad. |

La piattaforma Qt `epaper` attiva i servizi del dispositivo. L’opzione `--tablet` abilita il layout tablet e la tastiera a schermo anche su macOS. Durante la configurazione, CMake adatta le proprietà QML di accessibilità al kit Qt del firmware. [Modulo QML con CMake](https://doc.qt.io/qt-6/qt-add-qml-module.html) · [Qt Quick sui tablet reMarkable](https://developer.remarkable.com/documentation/qt_epaper)

## Implementazione di diagrammi e orientamento (0.2.8)

Il modello [Notebook](../src/notebook.cpp) indicizza i diagrammi insieme a grafici e tabelle. `createDiagramBook()` crea una pagina realmente vuota e il metadato facoltativo `workspace: "diagrams"`. `diagramBook` identifica quel quaderno. `openLibraryItem()` restituisce `workspace: true` per la voce sintetica vuota; la navigazione apre la pagina senza creare un oggetto diagramma vuoto o aprirne l’editor. Gli aggiornamenti conservano la classificazione locale esistente se un’app precedente omette il campo.

[`DiagramRecognition`](../src/diagramrecognizer.cpp) analizza la geometria in modo locale e deterministico. `Notebook::diagramHandwritingPlan()` aggiunge identità di quaderno/pagina/livello e revisione corrente dell’indice. `applyDiagramHandwriting()` verifica nuovamente sorgente e struttura rivista prima di una sola modifica atomica a tratti e oggetti. Sono ammesse solo le modifiche previste al testo delle etichette. La sostituzione rimuove l’unione degli indici riconosciuti e delle etichette accettate non vuote, deduplicando i candidati sovrapposti. I tratti non assegnati rimangono. Il campo limitato `handwritingSource` permette il recupero dopo la riapertura; l’annullamento di sessione è separato. Le copie indipendenti dalla libreria omettono tale sorgente. [`DiagramExport`](../src/diagramexport.cpp) condivide la scrittura QIODevice fra `.drawio` singolo e allegati Markdown preparati in una cartella temporanea.

[`ScreenOrientation`](../src/screenorientation.cpp) legge l’accelerometro IIO supportato sul Paper Pro attraverso un worker, verifica la matrice di montaggio e filtra campioni ambigui o instabili. Una sola lettura può essere in corso. Un errore azzera la stabilità del candidato; errori ripetuti ritentano il rilevamento senza inventare un orientamento. Non serve Qt Sensors. Main abilita il campionamento solo con preferenza attiva, app sveglia e PIN sbloccato. La rotazione esistente di scena logica e overlay gestisce coordinate di penna e tocco. L’orientamento di lettura fisso ha precedenza; il cambio attende durante un’interazione o operazione modale. Scegliere un angolo manuale disattiva la rotazione automatica, inizialmente spenta.

Le proposte automatiche dei diagrammi sono separate dai salvataggi: una pausa può mostrare un suggerimento richiudibile solo nel quaderno Diagrammi. Rimane obbligatoria la revisione esplicita. Vedi [Diagrammi](DIAGRAMMI.md) e [impostazioni](IMPOSTAZIONI.md). La prima build 0.2.8 ha superato tutte le 15 suite Mac e ARM, regressioni geometriche/UI, controlli dei pacchetti e avvio nativo. L’utente ha confermato la rotazione automatica da verticale a orizzontale, poi un disegno reale ha evidenziato un riconoscimento geometrico troppo restrittivo. Le tolleranze riviste coprono imperfezioni limitate dei contorni e ulteriori gesti per le punte delle frecce; la geometria ambigua resta inchiostro. Una correzione separata delle notifiche di sincronizzazione ha superato le tre suite interessate su ogni piattaforma. Le suite corrette Mac e ARM di geometria/UI sono superate 2/2 (34 e 17 verifiche), con avvii rigorosi; la build corretta è installata sul Paper Pro e ha superato l’avvio rigoroso nativo/AppLoad a 1620 × 2160 senza errori QML. Il 18 novembre 2026 l’utente ha confermato che riconoscimento, applicazione del risultato e apertura dell’editor per modificarlo funzionano nel diagramma di prova sul Paper Pro; vedi lo [stato delle verifiche](IMPOSTAZIONI.md#ambito-delle-verifiche).

## Compilare su Mac

Servono CMake 3.21.1 o successivo, gli strumenti di compilazione di Xcode, OpenSSL 3, zlib e Qt 6.5 o successivo compatibile con il compilatore e con l’SDK Apple in uso. Installa Qt Quick, Qt Quick Controls, Qt Quick Dialogs, Qt Network, Qt D-Bus e Qt Linguist Tools. La build Mac verificata usa Apple Silicon, SDK macOS 27.2 e Qt 6.8.3 con la correzione al collegamento di AGL. Lo ZIP ottenuto ha come requisito minimo macOS 27.2. Se OpenSSL è fuori dai percorsi di ricerca predefiniti, imposta `OPENSSL_ROOT_DIR` in CMake.

Imposta `QT_PREFIX` al percorso della tua installazione Qt per macOS e configura una build Release. Il percorso nella prima riga è un esempio per un’installazione tramite Qt Online Installer. Se il linker riporta `framework 'AGL' not found`, usa un kit Qt che include la correzione ufficiale e configura una nuova directory di build. [Piattaforme macOS supportate da Qt](https://doc.qt.io/qt-6/macos.html) · [Correzione Qt per AGL](https://github.com/qt/qtbase/commit/cdb33c3d)

```bash
QT_PREFIX="$HOME/Qt/6.8.3/macos"
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX" -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-mac --parallel
open build-mac/quaderno.app
```

Con l’SDK macOS 27 o successivo e Swift, CMake compila anche `quaderno-handwriting` e lo copia in `Contents/MacOS` accanto all’app. La funzione **Trascrivi in italiano** è disponibile quando il sistema è macOS 27 o successivo e il processo ausiliario è presente ed eseguibile. Il requisito minimo dell’app dipende dal kit Qt e dal target di distribuzione usati per compilarla. [Riconoscimento della scrittura con PencilKit](https://developer.apple.com/documentation/pencilkit/recognizing-handwriting-and-converting-to-text)

```bash
xcrun --sdk macosx --show-sdk-version
file build-mac/quaderno.app/Contents/MacOS/quaderno
xcrun vtool -show-build build-mac/quaderno.app/Contents/MacOS/quaderno
ls -l build-mac/quaderno.app/Contents/MacOS/quaderno-handwriting
```

## Compilare per Paper Pro 3.29.1.0

Usa Linux o una VM Linux con CMake, Ninja, Bash e gli strumenti `file` e `readelf`. Scegli un SDK `ferrari` per l’architettura della macchina Linux che esegue la compilazione. Il firmware di riferimento è **3.29.1.0** e il kit usato per il collaudo è **Codex 6.0.112**. Il risultato è un ELF Linux AArch64. [Come scegliere il kit reMarkable](https://developer.remarkable.com/documentation/sdk) · [SDK Ferrari 3.29.1.0 per host Linux AArch64](https://storage.googleapis.com/remarkable-codex-toolchain/3.29.1.0/remarkable-production-image-6.0.112-ferrari-public-aarch64-toolchain.sh)

Dopo aver scaricato il kit, esegui l’installer e scegli la directory di destinazione. Adatta la prima riga al percorso effettivo di installazione. Usa una directory di build nuova se cambi SDK o generatore.

```bash
PAPER_PRO_SDK='/percorso/di/installazione/dell-sdk'
source "$PAPER_PRO_SDK/environment-setup-cortexa53-crypto-remarkable-linux"

cmake -S . -B build-paper-pro -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE" \
  -DQt6_DIR="$OECORE_TARGET_SYSROOT/usr/lib/cmake/Qt6" \
  -DQT_HOST_PATH="$OECORE_NATIVE_SYSROOT/usr" \
  -DCMAKE_SKIP_RPATH=ON
cmake --build build-paper-pro --parallel
file build-paper-pro/quaderno
readelf -l build-paper-pro/quaderno
readelf -d build-paper-pro/quaderno
```

Verifica che `file` riporti un eseguibile ELF a 64 bit per AArch64. `readelf` mostra l’interprete dinamico e le dipendenze. Controlla che l’eseguibile sia privo di riferimenti `RPATH` o `RUNPATH` a percorsi della macchina di compilazione. Sul tablet `try-on-device.sh --check` controlla l’eseguibile, la risoluzione delle librerie e la disponibilità dei plugin Qt e‑ink.

Per attivare la trascrizione offline in italiano sul Paper Pro, compila il motore WritePad separato con lo stesso SDK. Metti l’eseguibile e il dizionario italiano accanto a `quaderno` nel pacchetto. L’installatore verifica che il motore possa caricare il dizionario. Quando distribuisci il motore, includi gli avvisi di licenza e il codice sorgente corrispondente. [Compilare il motore di trascrizione](../tools/writepad/README.md)

## Verificare le modifiche

Il repository contiene i sorgenti e gli script di installazione. Le verifiche funzionali sono riproducibili con i passaggi qui sotto. Per i test usa quaderni, immagini e credenziali di prova. I target di regressione 0.2.8 includono `diagramrecognition_tests`, `diagramsintegration_tests` e `screenorientation_tests` per geometria, conversione/recupero atomici, flussi di libreria/interfaccia e filtro del sensore. La loro presenza non attesta il superamento del collaudo sul dispositivo.

GitHub Actions compila l’app desktop su Linux e controlla l’avvio nei layout desktop e tablet. Per ripetere il controllo degli script in locale, installa ShellCheck ed esegui il comando seguente.

```bash
sh scripts/check-shell.sh
```

Per una prova visiva del layout tablet su Mac, avvia il binario di sviluppo direttamente da Terminale. Chiudi prima altre istanze di Quaderno se vuoi usare la stessa libreria.

```bash
build-mac/quaderno.app/Contents/MacOS/quaderno --tablet
```

Il controllo di avvio usa un profilo temporaneo e il plugin `offscreen` del kit Qt. Indica l’eseguibile compilato e, su macOS, la cartella dei plugin del kit Qt.

```bash
QT_PLUGIN_PATH="$QT_PREFIX/plugins" \
  python3 scripts/check-startup.py build-mac/quaderno.app/Contents/MacOS/quaderno
# Su Linux
python3 scripts/check-startup.py build-ci/quaderno
```

| Area | Verifica manuale |
| --- | --- |
| Libreria e persistenza | Crea un quaderno con più pagine, tag, cartelle e livelli. Riapri l’app e verifica i dati. Ripeti un’importazione dello stesso JSON e prova un conflitto con una copia modificata. |
| Penna e livelli | Scrivi, cancella, annulla e ripeti. Nascondi e mostra un livello. Verifica le forme intelligenti, il lazo e il ripristino del tratto originale dopo la riapertura. |
| Grafici e documenti | Seleziona due assi e una curva o barre. Correggi i dati, imposta la scala e confronta il CSV con l’anteprima. Importa un PDF e un EPUB senza DRM, aggiungi annotazioni e controlla il PDF esportato. |
| Trascrizione | Usa un breve campione scritto a mano. Verifica che l’anteprima compaia in automatico sul Paper Pro. Rivedi il testo, salvalo su una nuova pagina, cercalo e verifica che i tratti originali siano ancora disponibili. |
| Diagrammi | Crea un quaderno Diagrammi vuoto e riaprilo; disegna e rivedi un flusso, escludi un’etichetta, applica, annulla/ripeti e recupera i tratti dopo il riavvio. Inserisci una copia in Scrittura e verifica indipendenza, PNG/`.drawio` nel Markdown e trasferimento del quaderno. |
| Orientamento automatico | Con sensore supportato verifica quattro angoli stabili, posizioni piatte/ambigue, allineamento degli ingressi, attesa con finestre, PIN/sospensione, scelta manuale e orientamento fisso del libro. Un errore di lettura non deve generare un nuovo angolo. |
| Paper Pro fisico | Verifica coordinate e pressione del Marker, allineamento touch, refresh e leggibilità e‑ink, luce di lettura e Wi‑Fi. Prova **Esci** e la scadenza della sessione. Verifica il ritorno dell’interfaccia reMarkable in entrambi i casi. |

La build per il firmware è stata verificata anche in una VM Linux AArch64 e su un Paper Pro fisico. Ripeti le prove sul dispositivo per ogni pacchetto scaricabile. [Stato del rilascio e del collaudo](RILASCIO.md) · [Prova via USB](INSTALLAZIONE.md)

### Aggiornamento del display e‑ink sul Paper Pro

Quaderno avvia Qt Quick con il backend `epaper`. Durante la scrittura, `InkItem` ridisegna il rettangolo del tratto modificato. La gomma ricostruisce invece il livello di inchiostro quando trova un tratto da rimuovere. Un ridisegno di Qt aggiorna il contenuto della scena. La pulizia delle immagini residue (*ghosting*) dipende dall’aggiornamento fisico del pannello Gallery 3. [Qt Quick su reMarkable](https://developer.remarkable.com/documentation/qt_epaper) · [QQuickPaintedItem::update](https://doc.qt.io/qt-6/qquickpainteditem.html#update) · [Display Gallery 3](https://blog.eink.com/new-remarkable-paper-pro)

Nel plugin `libqsgepaper.so` del firmware **3.29.1.0**, `scheduleGhostRemoval()` accoda una pulizia completa per il prossimo aggiornamento fisico. Se i pixel restano identici, la richiesta resta in attesa. `clearGhosting()` aggiorna subito l’intero pannello usando il fotogramma corrente e attende il controller. Entrambi i metodi appartengono al plugin interno del firmware.

Quaderno abilita **Aggiorna schermo** sul Paper Pro con firmware 3.29.1.0 quando il plugin caricato espone `clearGhosting()`. I cambi pagina usano il normale ridisegno di Qt. Se Qt tarda a elaborare il fotogramma, l’opzione automatica richiede un aggiornamento tramite `QWindow::requestUpdate()`. Sul tablet le finestre modali mantengono visibile lo sfondo senza oscurarlo, così aprire un pannello ridisegna una zona più piccola. [QWindow::requestUpdate](https://doc.qt.io/qt-6/qwindow.html#requestUpdate)

Sul firmware verificato, Quaderno usa le modalità `UI` e `Content` del plugin per le normali aree ridisegnate da Qt. La modalità `UI` copre interfaccia e pagine di appunti. La modalità `Content` copre le pagine con un documento, un’immagine, un grafico o un diagramma. Il cambio di modalità usa il prossimo fotogramma e il controller sceglie la forma d’onda in base alle condizioni del pannello.

Per il collaudo fisico, passa da una pagina con tratti neri e colorati a una pagina bianca, sfoglia rapidamente un PDF e cancella un gruppo di tratti. Prova anche **Aggiorna schermo** con una pagina ferma, sia nell’editor sia in **Pagina intera**. Confronta le voci di `QT_LOGGING_RULES='rm.framebuffer.updates.debug=true'` con un video del display a esposizione fissa. Misura il tempo dal tocco alla pagina leggibile, le immagini residue e il numero di lampeggi. Il segnale Qt `frameSwapped` indica che il fotogramma è stato accodato. Il video mostra quando il pannello si è stabilizzato. [QQuickWindow::frameSwapped](https://doc.qt.io/qt-6/qquickwindow.html#frameSwapped)

### Accessori Bluetooth sul Paper Pro

Quaderno gestisce ricerca e abbinamento attraverso BlueZ. L’agente di abbinamento di Quaderno gestisce i codici per le operazioni avviate dall’app. Il firmware gestisce il profilo HID e Qt riceve gli eventi di tastiera dal sistema. Per il collaudo, abbina una tastiera in modalità rilevabile, digita in Markdown e in una cella, quindi prova disconnessione, riconnessione e dissociazione. [API Adapter e Device di BlueZ](https://github.com/bluez/bluez/blob/master/doc/org.bluez.Adapter.rst) · [API Agent di BlueZ](https://github.com/bluez/bluez/blob/master/doc/org.bluez.Agent.rst)

## Preparare gli archivi di distribuzione

### Mac Apple Silicon

Esegui `macdeployqt` su una copia del bundle di sviluppo. L’opzione `-qmldir` permette a Qt di includere i plugin QML usati dall’app. Se la distribuzione Qt ha plugin opzionali con dipendenze mancanti, verifica quali plugin servono a Quaderno e rimuovi dalla copia solo quelli estranei al progetto prima di firmare. [Distribuzione delle app Qt su macOS](https://doc.qt.io/qt-6/macos-deployment.html)

```bash
mkdir -p build-mac/distribuzione
APP='build-mac/distribuzione/Quaderno.app'
ditto build-mac/quaderno.app "$APP"
"$QT_PREFIX/bin/macdeployqt" "$APP" -qmldir="$PWD/qml"

# Firma ad hoc per collaudi locali.
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"
ditto -c -k --keepParent "$APP" build-mac/Quaderno-macOS.zip
unzip -tq build-mac/Quaderno-macOS.zip
shasum -a 256 build-mac/Quaderno-macOS.zip
```

Per una distribuzione pubblica macOS, documenta lo stato della firma e, se usi Developer ID, firma l’app e i componenti incorporati e completa la notarizzazione secondo le istruzioni Apple. La firma ad hoc del comando precedente è per il collaudo locale. [Firma e notarizzazione macOS](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)

### Paper Pro

Usa lo script di preparazione con una build Linux AArch64 e una nuova cartella di destinazione. Servono `zip`, `unzip` e un programma per il calcolo SHA-256.

```bash
sh scripts/package-paper-pro.sh build-paper-pro/quaderno build-paper-pro/packages
```

Per includere il motore di trascrizione offline, esegui lo script nell’ambiente dell’SDK corrispondente e passa l’archivio WritePad della revisione indicata come terzo argomento. Distribuisci l’archivio dei sorgenti che lo script crea insieme allo ZIP. [Compilazione e pacchetto WritePad](../tools/writepad/README.md)

Per verificare il pacchetto, estrailo su una macchina di prova ed esegui l’installazione USB con `--install-only`. Poi avvia una sessione temporanea e verifica il ritorno all’interfaccia reMarkable. Per ogni build distribuita indica il commit, la versione del firmware collaudata e lo SHA‑256 dello ZIP.

## Protocollo di sincronizzazione incrementale

Consulta [Sincronizzazione wireless incrementale](PROTOCOLLO_SYNC.md) per negoziazione, manifesti di pagine e risorse, conferme delle parti salvate, ripresa, limiti della cache e test.
