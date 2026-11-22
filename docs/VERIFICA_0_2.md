# Quaderno 0.2 — registro delle verifiche

[English](en/VALIDATION_0_2.md) · [Note di sviluppo](AGGIORNAMENTO_0_2.md)

Registrato il **16 novembre 2026** per la copia di lavoro in sviluppo. Il documento distingue le verifiche completate da quelle ancora necessarie sui pacchetti finali.

## Stato delle build finali

| Verifica | Stato |
| --- | --- |
| Build macOS Release finale e tutte le sei suite automatiche | **Superate: 6/6 suite, 45 casi di prova più 12 di inizializzazione/chiusura; 13,18 s** |
| Avvio macOS con rendering dei layout desktop e tablet | **Superato con profili isolati, anche con l’app completa delle dipendenze** |
| Pacchetto macOS | **Firma locale di sviluppo verificata; dipendenze incluse controllate. Compilato per macOS 27.2 o successivo, Apple Silicon** |
| Build finale con SDK Paper Pro e suite automatiche ARM | **Superate: 6/6 suite, 44 casi di prova più 12 di inizializzazione/chiusura; 11,11 s. Saltata come previsto una prova di consegna Apple riservata a macOS** |
| Avvio ARM con rendering dei layout desktop e tablet | **Superato in una VM con il runtime Qt del firmware** |
| Installazione e prova sul Paper Pro fisico | **In attesa del collegamento del dispositivo** |

La build ARM usa SDK ferrari 6.0.112, Qt 6.10.3 e GCC 16.2 per il firmware 3.29.1.0. I log finali restano localmente in `build-dev/test-final.log` e `build-arm/test-final.log`.

Questo registro non attesta l’installazione della nuova build su un Paper Pro fisico. L’esecuzione ARM in una VM e il layout tablet sul Mac non verificano refresh del display reale, Marker, tocco, Bluetooth o prestazioni del dispositivo.

## Verifiche del lettore su macOS

Il lettore è stato provato su Apple Silicon con macOS 27.2 e Qt 6.8.3. Il programma di verifica isolato usava una build **Debug**.

Un EPUB di prova ha prodotto **591 pagine** in **3,839 secondi**. L’importazione ha conservato l’archivio EPUB originale e la relativa impaginazione PDF vettoriale. Durante l’importazione è stata preparata l’impaginazione, senza rasterizzare tutte le pagine.

Le pagine **1, 11, 76 e 151** sono state poi richieste singolarmente e renderizzate a **1350 × 1800 pixel**. Tutte e quattro contenevano elementi visibili. Sono state ispezionate anche visivamente la copertina e la pagina 11: erano presenti l’illustrazione della copertina e il testo della pagina.

La suite QtTest mirata del lettore ha riportato **8 verifiche superate, 0 fallite e 0 saltate**, incluse inizializzazione e chiusura. Ha coperto:

- Un PDF generato di 161 pagine, rendering della pagina richiesta, risoluzione dello zoom e rifiuto di un indice pagina non valido.
- Testo EPUB e un’immagine contenuta in markup SVG.
- Annullamento e rifiuto di un sorgente con impronta errata.
- Conservazione del sorgente originale e delle annotazioni dopo riapertura, spostamento di una pagina in un altro quaderno e trasferimento a un altro modello.
- Importazione asincrona che mantiene pagina corrente e cronologia Annulla, con annullamento senza quaderni parziali.
- Ciclo eventi reattivo durante la preparazione dell’importazione.

Questi risultati verificano i percorsi di rendering provati. Non attestano il supporto a ogni impaginazione EPUB o alle pubblicazioni protette da DRM.

## Migrazione di una copia della libreria esistente

Il programma di verifica ha copiato una libreria da **62.513.311 byte**, circa **62,5 MB / 59,6 MiB**, e il relativo file delle posizioni di lettura in una cartella isolata. Ha caricato **11 quaderni esistenti**. Il libro attivo conteneva **471 pagine** e il suo indice di lettura **9**, corrispondente alla pagina **10**, è stato conservato.

Nella copia sono stati creati un nuovo quaderno di prova e un tratto di penna. Dopo salvataggio e riapertura:

- Le **impronte di revisione del contenuto degli 11 quaderni originali** coincidevano con quelle precedenti alla migrazione.
- Il quaderno di prova e il tratto erano ancora presenti.
- L’impronta SHA-256 del file effettivo della libreria dell’utente era invariata.

| Operazione sulla libreria copiata | Tempo osservato sul Mac |
| --- | ---: |
| Caricamento iniziale | 1,152 s |
| Migrazione e primo salvataggio | 2,603 s |
| Salvataggio incrementale successivo di un tratto | 0,140 s |
| Riapertura della libreria migrata | 1,082 s |

Sono singole osservazioni del programma di verifica sul Mac, non benchmark ripetuti né misure del Paper Pro. La prova non ha modificato la libreria effettiva dell’utente.

## Verifiche del salvataggio

La suite mirata di regressione ha riportato **13 verifiche superate, 0 fallite e 0 saltate**, incluse inizializzazione e chiusura. Le prove del salvataggio comprendevano migrazione del formato precedente, scrittori concorrenti, indici danneggiati o mancanti, oggetti pagina mancanti e conservazione della generazione precedente recuperabile.

Una prova dedicata al salvataggio incrementale usava **12 MiB di testo invariato per immagini e sorgenti**. La modifica successiva ha codificato **zero byte di risorse**, prodotto **un solo oggetto pagina modificato** e lasciato invariata la data di modifica dell’oggetto immagine già salvato. Un altro caso ha rifiutato una libreria espansa troppo grande anche quando i libri ripetuti condividevano le stesse risorse. Le voci della cache sono state rimosse quando le risorse non erano più referenziate.

## Evidenze e verifiche rimanenti

I test riproducibili sono nei sorgenti [del lettore](../tests/reader_tests.cpp) e [delle regressioni](../tests/regression_tests.cpp). L’esecuzione locale ha conservato i log nella cartella ignorata `build-reader-check`: `reader-final.log`, `regression-final.log`, `blue-team.log` e `migration.log`.

L’EPUB personale, le anteprime renderizzate e la copia della libreria sono materiale di verifica locale escluso dal repository. I risultati dei pacchetti finali vanno registrati nella tabella iniziale. Restano necessarie le prove sul Paper Pro fisico per gesture, refresh, interazione nativa con la penna, trasferimento wireless e prestazioni della build installata.
