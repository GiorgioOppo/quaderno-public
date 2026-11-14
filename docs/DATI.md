# Dati, backup e privacy

[English](en/DATA.md) · **Italiano**

## Dove sono salvati i file

| Piattaforma | Cartella dati predefinita |
| --- | --- |
| macOS | `~/Library/Application Support/Quaderno/` |
| Paper Pro | `/home/root/.local/share/Quaderno/` |

`quaderni.json` contiene la libreria di Quaderno, con cartelle, pagine, livelli, tag, tratti, testo e sfondi dei documenti importati. `accesso.json` contiene la configurazione del PIN, quando presente. I percorsi seguono la cartella dati dell’app restituita da Qt. Sul Paper Pro, l’importazione USB integrata legge il percorso predefinito della tabella.

L’app salva automaticamente le modifiche riuscite. Prima di aggiornare Quaderno, fai una copia della libreria. Le versioni precedenti dell’app possono rifiutare una libreria che contiene dati introdotti da una build più recente, come grafici o tratti originali delle forme corrette.

## Fare una copia

1. Chiudi Quaderno sul dispositivo da copiare.
2. Sul Mac apri la cartella dati dal Finder con **Vai → Vai alla cartella** e copia `quaderni.json` in una cartella di backup.
3. Sul Paper Pro in modalità sviluppatore, copia lo stesso file via USB‑C con OpenSSH. Esegui il comando da una cartella di backup e scegli un nome di destinazione libero.

```sh
scp -O root@10.11.99.1:/home/root/.local/share/Quaderno/quaderni.json ./quaderni-paper-pro-backup.json
```

Il backup del contenuto è `quaderni.json`. Conserva `accesso.json` separatamente e in un luogo protetto se vuoi mantenere anche il PIN e le relative impostazioni. Una copia del JSON di Quaderno è distinta da una copia dei documenti gestiti dall’interfaccia reMarkable.

## Ripristinare o importare

- Per **ripristinare tutta la libreria**, chiudi l’app e conserva una copia del file corrente. Copia il backup nella cartella dati con il nome `quaderni.json`, quindi riapri Quaderno. Se la cartella dati non esiste, avvia e chiudi l’app prima di ripristinare il file, oppure crea la cartella.
- Per **aggiungere quaderni a una libreria esistente** sul Mac, scegli **Importa quaderni** e apri il JSON, oppure scegli **Importa dal Paper Pro** per leggerlo via USB‑C. I quaderni invariati vengono riconosciuti. Quando il contenuto diverge e Quaderno deve conservare entrambe le versioni, l’importazione aggiunge una copia.
- L’importazione della libreria accetta file fino a **64 MiB**. Per una libreria più grande, conserva una copia completa del file. Il ripristino sostituisce la libreria aperta da Quaderno.

Per usare i contenuti in altre app, esporta le pagine in PDF, il testo in Markdown con gli eventuali allegati, le celle o i dati calibrati in CSV e i diagrammi in `.drawio`. [Formati e trasferimenti](FUNZIONALITA.md#formati-e-trasferimenti)

## Versioni della libreria

Il campo `version` di `quaderni.json` descrive il formato del file. L’app sceglie la versione in base ai contenuti effettivi della libreria.

| Versione | Contenuti introdotti |
| --- | --- |
| `1` | Quaderni e pagine con tratti d’inchiostro. |
| `2` | Modelli di pagina, blocchi di testo e tabelle. |
| `3` | Cartelle, preferiti, tag, cestino, livelli, strumenti di scrittura e sfondi importati. |
| `4` | Grafici e forme intelligenti con il tratto originale recuperabile. |
| `5` | Segnalibri di pagina, testo ricercabile dei PDF, trascrizioni sulle pagine originali e grafici collegati alle tabelle. |
| `6` | Diagrammi con forme, etichette e frecce collegate. |
| `7` | Diagrammi con frecce ad angolo retto. |
| `8` | Immagini PNG e JPEG incorporate come elementi modificabili nelle pagine. |

La build corrente legge i formati da 1 a 8. Aggiorna entrambe le app prima di trasferire quaderni che contengono immagini incorporate. [Implementazione del formato](../src/notebook.cpp)

I campi facoltativi `pageIds` e `lastViewedPageId` identificano le pagine e conservano l’ultima posizione locale per quaderno. Le librerie precedenti ricevono identificatori al caricamento. La posizione di lettura resta sul dispositivo e le modifiche di posizione non cambiano la revisione del contenuto.

## Privacy e accesso

- La libreria risiede sul dispositivo. Quando condividi o salvi un PDF, la destinazione scelta riceve una copia. Le destinazioni Apple gestiscono le proprie impostazioni iCloud.
- La trascrizione usa PencilKit e Vision sul Mac. L’app passa i tratti selezionati a un componente locale. [Documentazione Apple](https://developer.apple.com/documentation/pencilkit/recognizing-handwriting-and-converting-to-text)
- Il PIN protegge l’accesso nell’interfaccia. Quaderno conserva nel file di configurazione un hash PBKDF2 con salt casuale e limita i tentativi. `quaderni.json` è un file JSON leggibile da chi può accedere alla cartella dati. La protezione a riposo dipende dal sistema e da dove conservi le copie.
- La password SSH viene richiesta dal flusso di accesso quando serve. Per l’importazione dal Mac, OpenSSH verifica l’identità del tablet tramite la chiave host registrata in `known_hosts`. Quaderno passa le richieste Wi‑Fi al servizio del sistema sul Paper Pro.

Per una segnalazione di sicurezza o un esempio da allegare a un bug, usa un quaderno fittizio. [Politica di sicurezza](../SECURITY.it.md)
