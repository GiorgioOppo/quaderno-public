# Quaderno

[English](README.md) · **Italiano**

Quaderno è un’app nativa sperimentale per prendere appunti su **reMarkable Paper Pro** e **Mac Apple Silicon**. Riunisce scrittura a penna, testo, tabelle e annotazioni di documenti in una libreria salvata sul dispositivo.

![Editor di Quaderno con un grafico vettoriale su una pagina di esempio](docs/immagini/editor.png)

*Schermata della build Mac con dati di esempio.*

## Cosa puoi fare

- Esplorare **Libri**, **Scrittura**, **Grafici** e **Tabelle**. Organizzare quaderni e libri in cartelle, filtrarli per preferiti o tag e recuperarli dal cestino.
- Scrivere in Markdown accanto a tratti a mano libera, immagini, grafici e tabelle. Usare cinque modelli di pagina, livelli d’inchiostro, forme intelligenti e il lazo.
- Importare file CSV o TSV in tabelle modificabili, calcolare somme e medie ed esportare i risultati in CSV.
- Creare grafici a linee e a barre da punti modificabili oppure collegarli a una tabella della stessa pagina. Convertire uno schizzo in un grafico modificabile ed esportare i dati calibrati in CSV.
- Disegnare diagrammi di flusso con forme e frecce modificabili, con esportazione `.drawio`. [Diagrammi di flusso](docs/GUIDA.md#diagrammi-di-flusso)
- Evidenziare, sottolineare e aggiungere note modificabili a PDF ed EPUB senza DRM a impaginazione fluida. Usare zoom e segnalibri, cercare il testo nei PDF appena importati ed esportare la copia annotata in PDF.
- Importare su Mac copie locali in PDF ed EPUB senza DRM da Apple Books e inviare la copia annotata al Paper Pro con Quaderno. [Apple Books sul Mac](docs/GUIDA.md#note-libri-e-icloud-sul-mac)
- Importare Markdown modificabile, anche da un URL HTTPS diretto. Esportarlo con immagini collegate per l’inchiostro, i grafici e le immagini incorporate. Su Mac puoi salvare il testo in un vault Obsidian. Su Mac e Paper Pro puoi copiare i collegamenti a un sito Obsidian Publish. [Markdown e Obsidian](docs/GUIDA.md#markdown-e-obsidian)
- Sincronizzare le note Markdown direttamente tra Paper Pro e Obsidian Sync via Wi‑Fi dopo la prima configurazione via USB‑C. [Obsidian Sync sul Paper Pro](docs/GUIDA.md#obsidian-sync-sul-paper-pro)
- Trasferire e sincronizzare quaderni tra Mac e Paper Pro tramite Bluetooth LE o Wi‑Fi locale. Il trasferimento via USB‑C è disponibile dalla libreria e dall’editor. Condividere copie PDF con Note dal menu del Mac, oppure salvare un PDF e importarlo in Apple Books.
- Cercare e abbinare accessori Bluetooth sul Paper Pro, come le tastiere HID. [Collegare una tastiera Bluetooth](docs/GUIDA.md#collegare-una-tastiera-bluetooth)
- Trascrivere offline la scrittura a mano in italiano con il motore per Paper Pro o su macOS 27 e successivi. Correggere il testo e cercarlo per riaprire la pagina manoscritta originale.
- Scegliere l’inglese o l’italiano per l’interfaccia. Sul Paper Pro puoi passare da QWERTY italiano a inglese USA mentre scrivi e modificare i blocchi di testo con un tocco.

## Compatibilità e stato

| Piattaforma | Stato della build |
| --- | --- |
| Mac Apple Silicon | La build locale attuale richiede **macOS 27.2 o successivo**. I requisiti delle build da sorgente dipendono dal kit Qt e dalla versione minima impostata in compilazione. |
| reMarkable Paper Pro (`ferrari`) | Firmware **3.29.1.0**. Quaderno è stato avviato su un tablet fisico con AppLoad, input nativo del Marker e refresh e‑ink. |

La prova sul Paper Pro usa la modalità sviluppatore. **Attivarla ripristina il tablet alle impostazioni di fabbrica.** Salva i tuoi documenti prima di attivarla. La sessione di Quaderno è temporanea e gli script avviano di nuovo l’interfaccia reMarkable alla chiusura. [Guida ufficiale alla modalità sviluppatore](https://developer.remarkable.com/documentation/developer-mode)

Quaderno salva la libreria in un formato locale. I trasferimenti USB‑C partono su richiesta. Per indicizzare il testo incorporato in un PDF già presente in libreria, occorre importarlo di nuovo.

## Per iniziare

La versione **0.1.0** è un’anteprima dei sorgenti. La guida al rilascio indica lo stato dei pacchetti scaricabili. [Preparazione del rilascio](docs/RILASCIO.md)

- **Sul Mac** apri `Quaderno.app` dal pacchetto Mac Apple Silicon, se disponibile, oppure compila dai sorgenti. [Installazione](docs/INSTALLAZIONE.md) · [Compilazione](docs/SVILUPPO.md)
- **Sul Paper Pro** controlla modello e firmware, quindi usa l’installazione via USB‑C descritta nella guida. [Installazione sul Paper Pro](docs/INSTALLAZIONE.md)
- **Nell’app** crea un quaderno, scegli un modello di pagina e scrivi. Il contatore in basso apre le miniature e le azioni sulle pagine. [Guida d’uso](docs/GUIDA.md)

## Documentazione

| Guida | Contenuto |
| --- | --- |
| [Installazione](docs/INSTALLAZIONE.md) | Mac, USB‑C, AppLoad, aggiornamento e ripristino |
| [Guida d’uso](docs/GUIDA.md) | Libreria, scrittura, grafici, trascrizione e documenti |
| [Sezioni e scambio dei file](docs/FUNZIONALITA.md) | Libri, Scrittura, Grafici, Tabelle, formati e riuso dei contenuti |
| [Dati e backup](docs/DATI.md) | Posizione dei file, copie di sicurezza, importazione e PIN |
| [Sviluppo](docs/SVILUPPO.md) | Architettura, compilazione e verifiche |
| [Preparazione del rilascio](docs/RILASCIO.md) | Stato dei pacchetti, collaudo e distribuzione |
| [Cronologia](CHANGELOG.it.md) | Funzioni incluse nella versione in sviluppo |

## Contributi e sicurezza

Per correzioni e segnalazioni consulta [CONTRIBUTING.it.md](CONTRIBUTING.it.md). Per problemi di sicurezza consulta [SECURITY.it.md](SECURITY.it.md).

Quaderno è un progetto indipendente. Il codice dell’app è disponibile con licenza [MIT](LICENSE). Il componente opzionale WritePad usa la licenza GPL-3.0-or-later. [Licenze dei componenti di terze parti](THIRD_PARTY_NOTICES.md)
