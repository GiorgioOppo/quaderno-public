# Quaderno

[English](README.md) · **Italiano**

Quaderno è un’app nativa sperimentale per prendere appunti su **reMarkable Paper Pro** e **Mac Apple Silicon**. Riunisce scrittura a penna, testo, tabelle e annotazioni di documenti in una libreria salvata sul dispositivo. **La 0.2.19 è un’anteprima dei sorgenti.** Compila questa versione dal repository. Il tag `v0.1.0` identifica l’anteprima originale. [Istruzioni di compilazione](docs/SVILUPPO.md) · [Stato del rilascio](docs/RILASCIO.md)

![Editor di Quaderno con un grafico vettoriale su una pagina di esempio](docs/immagini/editor.png)

*Schermata della build Mac con dati di esempio.*

## Cosa puoi fare

- Esplorare **Libri**, **Scrittura**, **Grafici**, **Tabelle** e **Diagrammi**. Organizzare quaderni e libri in cartelle, filtrarli per preferiti o tag e recuperarli dal cestino.
- Scrivere in Markdown accanto a tratti a mano libera, immagini, grafici e tabelle. Usare cinque modelli di pagina, livelli d’inchiostro, forme intelligenti e il lazo.
- Sul Paper Pro, disegnare con il Marker un contorno attorno alle aree di diagramma, tabella e testo. Il motore OCR offline opzionale propone testo in alfabeto latino, contenuti delle celle ed etichette delle forme da PDF e sfondi immagine. Rivedere le bozze e salvare un unico quaderno modificabile con collegamenti al diagramma e alla tabella. [Riconoscimento di una pagina mista](docs/GUIDA.md#riconoscere-una-pagina-mista-sul-paper-pro)
- Importare file CSV o TSV in tabelle modificabili, calcolare somme e medie ed esportare i risultati in CSV.
- Creare grafici a linee e a barre da punti modificabili oppure scegliere una tabella e le colonne X/Y. Una tabella sulla stessa pagina aggiorna il grafico automaticamente. Una sorgente su un’altra pagina fornisce una copia modificabile dei valori correnti. Convertire uno schizzo in un grafico modificabile ed esportare i dati calibrati in CSV.
- Costruire diagrammi di flusso con palette delle forme, connettori e ispettore nella sezione **Diagrammi**. Rivedere la proposta ricavata da un diagramma a mano libera, aggiungere forme e frecce a un diagramma esistente, modificarne forme, etichette e frecce e riusarlo in altre pagine. Esportare `.drawio` oppure Markdown con PNG del diagramma e allegato `.drawio` modificabile. [Diagrammi](docs/DIAGRAMMI.md)
- Evidenziare e sottolineare PDF ed EPUB senza DRM a impaginazione fluida. Collocare un **Post-it** in un punto della pagina, scriverci con tastiera o penna e riaprire il richiamo numerato per modificarlo. Inserire un collegamento in un punto della pagina per aprire un grafico, una tabella o un diagramma. Usare zoom e segnalibri, cercare il testo nei PDF appena importati ed esportare le pagine annotate con appendici leggibili per le note. [Note sui libri](docs/NOTE_LIBRI.md)
- Importare su Mac copie locali in PDF ed EPUB senza DRM da Apple Books e inviare la copia annotata al Paper Pro con Quaderno. [Apple Books sul Mac](docs/GUIDA.md#note-libri-e-icloud-sul-mac)
- Importare Markdown modificabile, anche da un URL HTTPS diretto. Esportarlo con immagini collegate per l’inchiostro, i grafici, i diagrammi e le immagini incorporate. Su Mac puoi salvare il testo in un vault Obsidian. Su Mac e Paper Pro puoi copiare i collegamenti a un sito Obsidian Publish. [Markdown e Obsidian](docs/GUIDA.md#markdown-e-obsidian)
- Sincronizzare le note Markdown direttamente tra Paper Pro e Obsidian Sync via Wi‑Fi dopo la prima configurazione via USB‑C. [Obsidian Sync sul Paper Pro](docs/GUIDA.md#obsidian-sync-sul-paper-pro)
- Trasferire e sincronizzare interi quaderni tra Mac e Paper Pro tramite Bluetooth LE o Wi‑Fi locale, comprese evidenziazioni, sottolineature e note modificabili su tutte le pagine dei libri. Scegliere il collegamento automatico e sincronizzare dopo le modifiche, all’apertura di un quaderno selezionato o a intervalli. Entrambe le app devono essere aperte, sbloccate e compatibili con il formato del quaderno. [Sincronizzazione wireless](docs/PROTOCOLLO_SYNC.md) Il trasferimento via USB‑C resta disponibile dalla libreria e dall’editor. Condividere copie PDF con Note dal menu del Mac, oppure salvare un PDF e importarlo in Apple Books.
- Cercare e abbinare accessori Bluetooth sul Paper Pro, come le tastiere HID. [Collegare una tastiera Bluetooth](docs/GUIDA.md#collegare-una-tastiera-bluetooth)
- Trascrivere offline la scrittura a mano in italiano e inglese con il motore per Paper Pro o su macOS 27 e successivi. Correggere il testo e cercarlo per riaprire la pagina manoscritta originale.
- Scegliere l’inglese o l’italiano per l’interfaccia. Sul Paper Pro puoi passare da QWERTY italiano a inglese USA mentre scrivi e modificare i blocchi di testo con un tocco.
- Personalizzare lettura e scrittura, strumenti, filtro del palmo, orologio, automazioni Wi-Fi, backup e cache. Per esplorare Google Drive, Dropbox e OneDrive servono le registrazioni OAuth di Quaderno e l’accesso al proprio account. [Impostazioni e servizi collegati](docs/IMPOSTAZIONI.md)
- Sul Paper Pro, attivare la rotazione automatica dell’interfaccia con un sensore supportato. Restano disponibili angoli manuali e orientamento di lettura per libro. [Impostazioni di rotazione](docs/IMPOSTAZIONI.md#rotazione-lettura-e-schermo-intero)

## Comandi per ogni sezione

La barra segue il lavoro che fai sulla pagina. I comandi frequenti sono in vista e quelli secondari sono in **Altro**.

| Sezione | Azioni principali |
| --- | --- |
| **Libri** | Aprire in modalità di lettura. Evidenziare, sottolineare, aggiungere Post-it e collegamenti agli oggetti della libreria. **Torna al libro** riapre la pagina di partenza. |
| **Scrittura** | Raggiungere penna, colore, spessore, preferiti e lazo in una barra compatta. Modificare Markdown, aggiungere testo e immagini e inserire contenuti dalla libreria. |
| **Grafici** | Scegliere punti manuali o una tabella di origine, selezionare le colonne X/Y, controllare l’anteprima, modificare dati e aspetto ed esportare CSV. |
| **Tabelle** | Modificare celle e formule, creare tabelle o importare CSV, inserire valori con penna e lazo, generare un grafico ed esportare CSV. |
| **Diagrammi** | Disporre forme, collegare nodi, modificarne le proprietà nell’ispettore, riconoscere diagrammi a mano libera e importare o esportare `.drawio`. |

Un’azione apre direttamente l’unico oggetto compatibile. Se la pagina contiene più candidati, scegli quello desiderato. Tabelle e grafici collegati, diagrammi e scrittura possono condividere la pagina. Apri un oggetto dalla sua sezione per usare la barra corrispondente. [Mappa completa di funzioni e flussi](docs/FUNZIONI_E_FLUSSI.md)

Per trasferire le annotazioni sincronizza il **libro di Quaderno**, comprese pagine e sorgente originale. Copiare soltanto il PDF o l’EPUB originale non trasferisce le annotazioni di Quaderno. Con l’automatismo all’apertura, le modifiche fatte mentre il libro rimane aperto attendono un nuovo evento; scegli **Dopo le modifiche** per sincronizzare automaticamente dopo i salvataggi. I nuovi Post-it richiedono la **0.2.11 su entrambe le app**. [Note e sincronizzazione dei libri](docs/NOTE_LIBRI.md)

## Compatibilità e stato

| Piattaforma | Stato della build |
| --- | --- |
| Mac Apple Silicon | Collaudato su **macOS 27.2**. I requisiti delle build da sorgente dipendono dal kit Qt e dalla versione minima impostata in compilazione. |
| reMarkable Paper Pro (`ferrari`) | Collaudato con firmware **3.29.1.0**, inclusi l’avvio a schermo intero con AppLoad e la selezione a mano libera con il Marker. |

La prova sul Paper Pro usa la modalità sviluppatore. **Attivarla ripristina il tablet alle impostazioni di fabbrica.** Salva i tuoi documenti prima di attivarla. Quaderno gira in una sessione separata e il launcher ripristina l’interfaccia reMarkable alla chiusura. La voce di menu opzionale richiede un’installazione compatibile di XOVI e AppLoad. [Guida ufficiale alla modalità sviluppatore](https://developer.remarkable.com/documentation/developer-mode) · [Configurazione AppLoad](docs/INSTALLAZIONE.md#avviare-dal-menu-appload)

Quaderno salva la libreria in un formato locale. I trasferimenti USB‑C partono su richiesta. Per indicizzare il testo incorporato in un PDF già presente in libreria, occorre importarlo di nuovo.

## Per iniziare

Sul Paper Pro con firmware 3.29.1.0, **Impostazioni → Accesso SSH** permette di scegliere se accettare nuove connessioni via Wi-Fi e USB-C. [Accesso SSH](docs/IMPOSTAZIONI.md#accesso-ssh-sul-paper-pro)

Usa la 0.2.14 o una versione successiva su entrambi i dispositivi per i libri con collegamenti (formato portabile 12). Se la destinazione è in un altro quaderno, trasferisci anche quel quaderno. La sincronizzazione automatica controlla gli aggiornamenti via Bluetooth e li trasferisce via Wi‑Fi locale quando è raggiungibile, con Bluetooth come riserva. [Sincronizzazione automatica](docs/PROTOCOLLO_SYNC.md#sincronizzazione-automatica)

- **Sul Mac** compila per Apple Silicon e apri `Quaderno.app`. [Compilazione](docs/SVILUPPO.md) · [Installazione](docs/INSTALLAZIONE.md)
- **Sul Paper Pro** controlla modello e firmware, prepara il pacchetto corrispondente e installalo via USB‑C. [Compilazione e pacchetto](docs/SVILUPPO.md#paper-pro) · [Installazione sul Paper Pro](docs/INSTALLAZIONE.md)
- **Nell’app** crea un quaderno, scegli un modello di pagina e scrivi. Il contatore in basso apre le miniature e le azioni sulle pagine. [Guida d’uso](docs/GUIDA.md)

## Documentazione

| Guida | Contenuto |
| --- | --- |
| [Migrazione di Quaderno 0.2](docs/AGGIORNAMENTO_0_2.md) | Migrazione da build precedenti e recupero dei dati |
| [Usabilità 0.2.1](docs/USABILITA_0_2_1.md) | 31 nuove migliorie, flussi d’uso, stato delle verifiche e sviluppi successivi |
| [Installazione](docs/INSTALLAZIONE.md) | Mac, USB‑C, AppLoad, aggiornamento e ripristino |
| [Guida d’uso](docs/GUIDA.md) | Libreria, scrittura, grafici, trascrizione e documenti |
| [Impostazioni e servizi collegati](docs/IMPOSTAZIONI.md) | Venti gruppi di impostazioni, rotazione automatica, riconoscimento dei diagrammi, lettura, automazioni e limiti per piattaforma |
| [Configurazione account cloud](docs/CLOUD_ACCOUNTS.md) | Registrazioni OAuth, assistente di accesso, trasferimenti manuali, sicurezza e prove reali ancora da svolgere |
| [Manutenzione della libreria](docs/LIBRARY_MAINTENANCE.md) | Backup pianificati, conservazione verificata, limiti cache e preferenze locali di lettura |
| [Sezioni e scambio dei file](docs/FUNZIONALITA.md) | Libri, Scrittura, Grafici, Tabelle, Diagrammi, formati e riuso dei contenuti |
| [Diagrammi](docs/DIAGRAMMI.md) | Riconoscimento a mano libera, revisione delle etichette, forme modificabili, recupero dei tratti e riuso |
| [Note sui libri](docs/NOTE_LIBRI.md) | Post-it ancorati, tastiera e scrittura a mano, sincronizzazione del libro, appendici PDF e allegati Markdown |
| [Sincronizzazione wireless](docs/PROTOCOLLO_SYNC.md) | Automatismi Wi-Fi/Bluetooth, libri annotati, controllo delle versioni e recupero dei trasferimenti |
| [Dati e backup](docs/DATI.md) | Posizione dei file, copie di sicurezza, importazione e PIN |
| [Sviluppo](docs/SVILUPPO.md) | Architettura, compilazione e verifiche |
| [Preparazione del rilascio](docs/RILASCIO.md) | Stato dei pacchetti, collaudo e distribuzione |
| [Cronologia](CHANGELOG.it.md) | Funzioni incluse nella versione in sviluppo |

## Contributi e sicurezza

Per correzioni e segnalazioni consulta [CONTRIBUTING.it.md](CONTRIBUTING.it.md). Per problemi di sicurezza consulta [SECURITY.it.md](SECURITY.it.md).

Quaderno è un progetto indipendente. Il codice dell’app è disponibile con licenza [MIT](LICENSE). Il componente opzionale WritePad usa la licenza GPL-3.0-or-later. [Licenze dei componenti di terze parti](THIRD_PARTY_NOTICES.md)
