# Dati, backup e privacy

[English](en/DATA.md) · **Italiano**

## Dove sono salvati i file

| Piattaforma | Cartella dati predefinita |
| --- | --- |
| macOS | `~/Library/Application Support/Quaderno/` |
| Paper Pro | `/home/root/.local/share/Quaderno/` |

`quaderni.json` è l’indice locale salvato atomicamente. Pagine, inchiostro, immagini e sorgenti dei documenti risiedono in `quaderni.json.objects/`. `quaderni.json.previous` conserva l’indice precedente; la migrazione iniziale mantiene anche `quaderni.json.legacy`. `document-sources/` è una cache ricostruibile dei sorgenti del lettore. Esportazioni Apple e coda si trovano in `apple-deliveries/`. `accesso.json` contiene la configurazione del PIN, quando presente. I percorsi seguono la cartella dati dell’app restituita da Qt. Sul Paper Pro, l’importazione USB integrata legge il percorso predefinito della tabella.

L’app salva automaticamente le modifiche riuscite. Prima di aggiornare Quaderno, fai una copia della libreria. Le versioni precedenti dell’app possono rifiutare una libreria che contiene dati introdotti da una build più recente, come grafici o tratti originali delle forme corrette.

## Fare una copia

1. Chiudi Quaderno sul dispositivo da copiare.
2. Sul Mac apri la cartella dati dal Finder con **Vai → Vai alla cartella** e copia l’intera cartella dati `Quaderno` in una cartella di backup.
3. Sul Paper Pro in modalità sviluppatore, copia l’intera cartella dati via USB‑C con OpenSSH. Esegui il comando da una cartella di backup e scegli un nome di destinazione libero.

```sh
scp -O -r root@10.11.99.1:/home/root/.local/share/Quaderno ./quaderno-paper-pro-backup
```

Il solo indice non contiene i contenuti. Conserva insieme indice, cartella degli oggetti e indice precedente. Il backup completo della cartella comprende anche configurazione PIN e abbinamenti. Esegui separatamente il backup dei documenti gestiti dall’interfaccia reMarkable.

## Ripristinare o importare

- Per **ripristinare tutta la libreria**, chiudi l’app e conserva una copia della cartella dati corrente. Ripristina l’intera cartella del backup e riapri Quaderno. Un vecchio JSON autosufficiente può ancora essere ripristinato come `quaderni.json`: verrà migrato al salvataggio successivo.
- Per **aggiungere quaderni a una libreria esistente** sul Mac, scegli **Importa quaderni** e apri il JSON, oppure scegli **Importa dal Paper Pro** per leggerlo via USB‑C. I quaderni invariati vengono riconosciuti. Quando il contenuto diverge e Quaderno deve conservare entrambe le versioni, l’importazione aggiunge una copia.
- L’importazione della libreria accetta file fino a **64 MiB**. Per una libreria più grande, conserva una copia completa della cartella dati. Il salvataggio locale ha un limite di 256 MiB di contenuto espanso. Il ripristino sostituisce la libreria aperta da Quaderno.

Per usare i contenuti in altre app, esporta le pagine in PDF, il testo in Markdown con gli eventuali allegati, le celle o i dati calibrati in CSV e i diagrammi in `.drawio`. [Formati e trasferimenti](FUNZIONALITA.md#formati-e-trasferimenti)

## Versioni della libreria

Il JSON portabile ha un campo `version` scelto secondo i contenuti. Il salvataggio locale usa `format: "quaderno-pages-v1"`, con metadati verificabili e riferimenti SHA-256 agli oggetti di pagine e risorse. Quaderno ricostruisce il JSON portabile per USB e wireless. Evita di modificare manualmente indice e oggetti.

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
| `9` | Sorgenti PDF/EPUB conservati, scrittura originale recuperabile dopo OCR, rotazione e scala degli elementi, lingua di riconoscimento per quaderno. |
| `10` | Scala visiva dei diagrammi ampliati (`styleScale`) e diagramma di base recuperabile per le aggiunte manoscritte (`handwritingSource.baseDiagram`). |
| `11` | Note ancorate alla pagina (`type: "note"`) con testo modificabile, scrittura a mano su un riquadro separato e colore del richiamo. |
| `12` | Collegamenti ancorati (`type: "link"`) a un grafico, una tabella o un diagramma della libreria Quaderno. |

I sorgenti della **0.2.14 in sviluppo** leggono i formati da 1 a 12. Un collegamento di pagina richiede il formato portabile 12. Una nota di pagina richiede il formato 11. Un elemento con `styleScale` oppure `handwritingSource.baseDiagram` richiede almeno il formato 10; i documenti senza contenuti nuovi mantengono il formato minimo necessario ai propri dati. I nuovi salvataggi locali usano invece metadati di versione 12 nell’indice `quaderno-pages-v1`. Il nome del formato locale e l’organizzazione degli oggetti non cambiano. Il lettore e il controllo di compatibilità wireless condividono la stessa versione massima supportata. [Implementazione del formato](../src/notebook.cpp) · [Limite di versione condiviso](../src/notebookformat.h)

Usa la **0.2.14 o successiva su entrambe le app** per trasferire quaderni con collegamenti tramite file, USB, Wi-Fi, Bluetooth o servizi cloud. I quaderni con note di pagina richiedono almeno la 0.2.11. I contenuti in formato 10 richiedono almeno la 0.2.10. Non abbassare il numero di versione del documento per forzare l’importazione: un lettore precedente non può conservare i contenuti più recenti. La sincronizzazione wireless controlla ora la capacità del destinatario prima di inviare le parti del contenuto; per i destinatari incrementali precedenti che non dichiarano la capacità, usa il formato 9 come limite prudenziale. [Protocollo di sincronizzazione](PROTOCOLLO_SYNC.md)

L’app installata sul Mac è la **0.2.0**. Sul Paper Pro è installata la **0.2.11**. Il backup è conservato sul tablet e l’esportazione della libreria è risultata identica prima e dopo l’aggiornamento. Per tornare a una versione precedente dell’app, conserva un backup completo precedente all’aggiornamento: un indice locale salvato dalla 0.2.14 usa i metadati 12 anche quando la singola esportazione portabile può usare una versione precedente.

I campi facoltativi `pageIds` e `lastViewedPageId` identificano le pagine e conservano l’ultima posizione locale per quaderno. Le librerie precedenti ricevono identificatori al caricamento. La posizione di lettura resta sul dispositivo e le modifiche di posizione non cambiano la revisione del contenuto.

## Note di pagina e libri annotati

Un Post-it è un elemento di pagina con `type: "note"` e un `id` stabile. I campi `x`, `y`, `width` e `height` posizionano il richiamo in coordinate normalizzate sulla pagina. `text` contiene testo semplice modificabile; `noteColor` è `yellow`, `blue`, `green` o `pink`. `noteInk` conserva tratti validati con `x`, `y` e pressione `p` normalizzati all’interno di un riquadro indipendente di 600 × 400. Spostare il richiamo ne cambia la posizione sul foglio senza ridimensionare la scrittura conservata. I numeri derivano dall’ordine delle note su ogni pagina e non sono identificatori permanenti.

Ogni nota accetta fino a 4.000 caratteri di testo, 2.000 tratti e 60.000 punti, nel limite già previsto di 120 elementi per pagina. Il salvataggio valida libro attivo, pagina e oggetto originale: una bozza di un’altra pagina o di una nota nel frattempo modificata non può sovrascrivere il contenuto corrente. Creazione, modifica, spostamento ed eliminazione partecipano ad Annulla/Ripeti della pagina. Il testo digitato è ricercabile e le note contrassegnano le pagine come annotate. [Note sui libri](NOTE_LIBRI.md)

Evidenziazioni e sottolineature usano già i normali tratti del quaderno. Un trasferimento nativo del libro di Quaderno comprende sorgente PDF/EPUB conservato, tutte le pagine, livelli d’inchiostro visibili e nascosti, elementi di pagina e testo/scrittura modificabili delle note. Copiare soltanto il sorgente originale non include queste annotazioni. Importare indipendentemente lo stesso PDF o EPUB sui due dispositivi crea libri separati: usa la sincronizzazione del libro intero per conservare l’identità condivisa del quaderno.

Entrambe le app devono essere aperte, sbloccate e collegate, con il quaderno desiderato selezionato per l’automatismo e il primo trasferimento già rivisto. L’automatismo all’apertura usa gli eventi previsti dalla sua modalità; scegli **Dopo le modifiche** per inviare le annotazioni salvate mentre il libro resta aperto. Se un’annotazione non arriva, controlla quindi collegamento, modalità, corrispondenza del libro di Quaderno e compatibilità delle versioni. Questi requisiti valgono sia per le evidenziazioni esistenti sia per le nuove note. [Sincronizzazione automatica](PROTOCOLLO_SYNC.md#sincronizzazione-automatica-nella-029)

L’esportazione PDF conserva i richiami numerati sulle pagine sorgenti selezionate, poi aggiunge appendici per le note con riferimento alla pagina e al numero originali. I testi lunghi sono impaginati; la scrittura a mano riceve una pagina separata. L’esportazione Markdown include il testo delle note e quella con allegati aggiunge un PNG della scrittura. Queste esportazioni sono copie leggibili; l’esportazione nativa di Quaderno o la sincronizzazione conservano la nota e i singoli tratti modificabili.

## Collegamenti di pagina

Un collegamento conserva la posizione e gli identificatori del quaderno, della pagina e dell’oggetto di destinazione nei campi `targetBookId`, `targetPageId` e `targetItemId`. Il campo `targetType` indica se l’oggetto è un grafico, una tabella o un diagramma. Quaderno apre l’oggetto originale sulla sua pagina e permette di tornare alla pagina di partenza. Quando la destinazione si trova in un altro quaderno, trasferisci anche quel quaderno con i suoi identificatori originali. Se manca la destinazione, il richiamo resta sulla pagina del libro. [Flussi fra libri e oggetti](FUNZIONI_E_FLUSSI.md#libri)

## Quaderni Diagrammi e tratti recuperabili

La 0.2.8 aggiunge il metadato facoltativo `workspace: "diagrams"` al quaderno. L’assenza del campo mantiene il comportamento precedente. Identifica un quaderno dedicato ai diagrammi anche quando la pagina è vuota o contiene solo inchiostro; la libreria genera una voce per il quaderno vuoto senza aggiungere nodi fittizi al documento. I diagrammi salvati restano normali elementi di pagina e compaiono nell’indice Diagrammi indipendentemente dalla sezione sorgente.

Il metadato viaggia nei quaderni portabili e negli indici locali. Un aggiornamento da un’app precedente conserva la classificazione di un quaderno Diagrammi locale già esistente se il campo manca in ingresso. Un’app precedente può ometterla alla prima importazione/esportazione: aggiorna entrambi i dispositivi per conservare la classificazione in modo uniforme. Pagine e inchiostro non dipendono da questa classificazione.

Quando il riconoscimento crea un diagramma e sostituisce i tratti, conserva gli originali rimossi nel campo limitato `handwritingSource`, che da solo richiede il formato portabile 9. Vengono rimossi solo geometria riconosciuta ed etichette accettate non vuote; l’altro inchiostro resta sulla pagina. **Ripristina scrittura originale** permette il recupero dopo la riapertura, separatamente da Annulla/Ripeti della sessione sulla pagina corrente.

La 0.2.10 può riconoscere nuove forme o sole frecce usando un diagramma esistente come contesto. L’applicazione sostituisce lo stesso elemento, mantenendone l’ID e le parti precedenti. L’anteprima contiene il diagramma completo; in presenza di più destinazioni serve una scelta esplicita, salvo che la selezione ne identifichi una sola. La creazione di un diagramma separato rimane disponibile. I tratti incerti o corrispondenti a parti già presenti non vengono consumati come nuove aggiunte.

Quando un’estensione rimuove inchiostro, `handwritingSource` conserva `layerId`, i tratti rimossi e una singola copia validata del diagramma precedente in `baseDiagram`. Estensioni successive sullo stesso livello accumulano i tratti mantenendo la stessa base, senza costruire una cronologia ricorsiva di diagrammi. Il recupero ripristina la base modificabile con l’ID dell’elemento corrente e restituisce i tratti delle aggiunte; non elimina il diagramma preesistente. Il campo `styleScale`, quando necessario per ampliare il riquadro, conserva la dimensione visiva di testo e linee. Uno qualsiasi di questi due campi nuovi richiede il formato portabile 10.

Con **Sostituisci i tratti convertiti con forme e testo** disattivato, tutto l’inchiostro originale resta sulla pagina e non viene aggiunta una nuova sorgente duplicata; eventuali dati di recupero già presenti restano conservati. Copiare un diagramma in un altro quaderno crea geometria ed etichette indipendenti senza la sorgente di recupero della pagina originale. Annulla/Ripeti ripristina insieme oggetto e tratti della singola applicazione nella sessione corrente; il recupero persistente torna invece alla base salvata delle estensioni.

L’esportazione Markdown con allegati salva ogni diagramma come anteprima PNG e file `.drawio` modificabile, con collegamenti relativi. La sintassi Markdown non incorpora oggetti Quaderno modificabili. Usa l’esportazione `.quaderno` o la sincronizzazione del quaderno per conservare elementi di pagina modificabili e tratti recuperabili. Il PDF mantiene l’aspetto, non la modifica degli oggetti. Vedi [Diagrammi](DIAGRAMMI.md).

Le preferenze per orientamento automatico e suggerimenti dei diagrammi restano locali al dispositivo e non viaggiano con gli oggetti. Anche le preferenze di lettura per libro restano separate dai contenuti sincronizzati. Vedi [manutenzione della libreria](LIBRARY_MAINTENANCE.md) per backup locali e cache.

## Privacy e accesso

- La libreria risiede sul dispositivo. Quando condividi o salvi un PDF, la destinazione scelta riceve una copia. Le destinazioni Apple gestiscono le proprie impostazioni iCloud.
- La trascrizione usa PencilKit e Vision sul Mac. L’app passa i tratti selezionati a un componente locale. [Documentazione Apple](https://developer.apple.com/documentation/pencilkit/recognizing-handwriting-and-converting-to-text)
- Il PIN protegge l’accesso nell’interfaccia. Quaderno conserva nel file di configurazione un hash PBKDF2 con salt casuale e limita i tentativi. Indice e oggetti delle pagine sono leggibili da chi può accedere alla cartella dati. La protezione a riposo dipende dal sistema e da dove conservi le copie.
- La password SSH viene richiesta dal flusso di accesso quando serve. Per l’importazione dal Mac, OpenSSH verifica l’identità del tablet tramite la chiave host registrata in `known_hosts`. Quaderno passa le richieste Wi‑Fi al servizio del sistema sul Paper Pro.

Per una segnalazione di sicurezza o un esempio da allegare a un bug, usa un quaderno fittizio. [Politica di sicurezza](../SECURITY.it.md)
