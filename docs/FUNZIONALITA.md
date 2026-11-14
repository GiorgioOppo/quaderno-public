# Sezioni di Quaderno e scambio dei file

[English](en/FEATURES.md) · **Italiano**

Quaderno ha quattro sezioni su Mac e Paper Pro. **Libri** e **Scrittura** aprono quaderni. **Grafici** e **Tabelle** elencano i singoli elementi salvati nelle pagine dei quaderni. Un grafico o una tabella può comparire sia nella propria sezione sia nella pagina originale. Aprirlo da uno dei due punti permette di modificare lo stesso elemento.

| Sezione | Da dove partire | Cosa salvare o condividere |
| --- | --- | --- |
| [Libri](#libri) | PDF locale o EPUB senza DRM a impaginazione fluida | Quaderno con annotazioni modificabili, oppure PDF per leggere e condividere |
| [Scrittura](#scrittura) | Documento Markdown vuoto o file Markdown esistente | Quaderno modificabile, file Markdown con eventuali allegati, oppure PDF |
| [Grafici](#grafici) | Punti inseriti nell’editor, tabella sulla stessa pagina o schizzo a mano | Grafico modificabile, dati calibrati in CSV, oppure immagine del grafico nell’esportazione Markdown |
| [Tabelle](#tabelle) | Griglia vuota o file CSV o TSV in UTF‑8 | Tabella modificabile, CSV delle celle, oppure grafico collegato alla tabella |

**File**, **Preferiti** e **Tag** filtrano la sezione aperta. Grafici e tabelle usano la cartella e lo stato di preferito del quaderno. Possono essere trovati con i tag del quaderno o della pagina. Il **Cestino** raccoglie i quaderni di tutte le sezioni. [Libreria e pagine](GUIDA.md#libreria-e-pagine)

## Libri

1. Scegli **Libri → Importa documento** e seleziona un PDF o un EPUB. Sul Mac, **Importa da Apple Books** offre due comandi. **Apri Apple Books** apre Libri. **Scegli PDF o EPUB…** seleziona copie locali, compresi gli EPUB che Finder mostra come un unico pacchetto. Se il PDF si apre in Anteprima, usa **File → Esporta** in Anteprima per salvarne una copia locale. La libreria iCloud e i titoli protetti restano accessibili in Apple Books. [PDF in Apple Books](https://support.apple.com/en-ge/guide/books/ibkseed72068/9.0/mac/27) · [Esportare da Anteprima](https://support.apple.com/guide/preview/export-pdfs-and-images-prvw0e8da223/mac)
2. Apri il libro importato. Usa **Evidenzia** per segnare un passaggio a mano libera. Scegli **Sottolinea** e trascina da un’estremità all’altra per tracciare una linea orizzontale. Usa **Aggiungi nota** per inserire un blocco di testo. Per modificare o spostare una nota salvata, attiva **Elementi** e tocca o trascina il blocco.
3. Usa lo zoom, **Adatta alla larghezza** e i segnalibri per consultare il libro. Quaderno ricorda l’ultima pagina su ogni dispositivo. **Cerca testo** usa il testo incorporato che il PDF rende estraibile. Per cercare in una scansione serve un livello di testo. Se hai importato il PDF con una build precedente, puoi importarlo di nuovo per indicizzarlo.
4. Scegli **Inserisci dalla libreria** per aggiungere una copia modificabile di un grafico o di una tabella. Usa **Salva PDF** sul Paper Pro o **Condividi → Salva PDF** sul Mac per ottenere una copia annotata. Trasferisci il quaderno con Quaderno per continuare a modificare le annotazioni sull’altro dispositivo.

Quaderno importa le pagine come sfondi. Evidenziazioni e sottolineature sono tratti d’inchiostro sovrapposti. L’importazione EPUB reimpagina il testo e conserva titoli, enfasi ed elenchi. Impaginazione originale, immagini e font incorporati restano fuori dalla copia. Puoi importare fino a **150 pagine** e **30 MiB** di immagini generate. Il file di partenza può essere un PDF fino a **48 MiB** o un archivio EPUB fino a **64 MiB**. Il PDF esportato riproduce gli elementi della pagina e i livelli visibili. Il livello di testo del PDF originale e gli oggetti di annotazione di Quaderno non sono disponibili come elementi separati nel file esportato. [Leggere e annotare documenti](GUIDA.md#leggere-e-annotare-pdf-ed-epub)

## Scrittura

1. Scegli **Scrittura → Nuovo Markdown**. Inserisci un titolo e digita il sorgente o salva un documento vuoto. Per partire da un file locale, usa **Importa documento** con un `.md` o `.markdown`. Il pannello **Markdown e Obsidian** accetta anche un collegamento HTTPS diretto a un file Markdown. L’importazione accetta testo UTF‑8 fino a **256 KiB** e **200 pagine**.
2. Scegli **Modifica Markdown** per cambiare il sorgente. Quaderno mostra la sintassi Markdown come testo sulle pagine e ridistribuisce il contenuto quando lo salvi. Le annotazioni esistenti rimangono sulle proprie pagine. Se il sorgente si accorcia, una pagina con annotazioni può restare nel quaderno anche se il suo testo sorgente è stato spostato o rimosso.
3. Scrivi con il Marker sul Paper Pro o trascina il puntatore sul Mac. Puoi scegliere tra cinque modelli, tre spessori e livelli d’inchiostro. Il lazo sposta, ruota, ridimensiona o copia i tratti selezionati. **Testo** aggiunge un blocco modificabile separato dall’editor del sorgente. Sul Paper Pro con il motore installato, il lazo avvia la trascrizione in italiano. Su un Mac compatibile, scegli **Trascrivi in italiano**. [Trascrivere la scrittura a mano](GUIDA.md#da-scrittura-a-mano-a-testo)
4. Usa **Inserisci immagine** per un PNG o JPEG locale. Quaderno incorpora una copia. In **Elementi**, tocca l’immagine per ridimensionarla o eliminarla, oppure trascinala per spostarla. Il file di partenza può essere di **16 MiB**. La copia incorporata arriva fino a **1.200 pixel** sul lato più lungo e **768 KiB**. Ogni pagina può contenere fino a **12 immagini**.
5. Usa **Inserisci dalla libreria** per aggiungere copie modificabili di grafici o tabelle, oppure crea una tabella o un diagramma nella pagina. Nell’editor dei diagrammi, l’importazione draw.io legge il primo diagramma di un file `.drawio` o `.xml` compatibile. Esporta in `.drawio` per modificare forme e collegamenti supportati in draw.io. [Diagrammi di flusso](GUIDA.md#diagrammi-di-flusso)

**Markdown e Obsidian → Esporta Markdown** salva il sorgente e i blocchi di testo. Quando presenti, aggiunge collegamenti relativi alle immagini incorporate e ai PNG dei grafici e dell’inchiostro visibile. Gli allegati sono in una cartella accanto al file. Sposta il Markdown e la cartella insieme. Reimportare il file recupera il testo e i collegamenti. Esporta le tabelle in CSV e i diagrammi in `.drawio` dai rispettivi editor. Per continuare a modificare gli oggetti sull’altro dispositivo, trasferisci il quaderno di Quaderno. L’integrazione con i vault Obsidian scambia il testo Markdown. [Markdown e Obsidian](GUIDA.md#markdown-e-obsidian)

## Grafici

- **Inserisci punti a mano.** In **Grafici → Nuovo grafico**, dai un nome al quaderno e parti da due punti modificabili. Scegli linee o barre, modifica il titolo e le etichette degli assi, aggiungi, rimuovi o correggi i punti. L’editor usa coordinate da **0 a 100%**. **Scala numerica** imposta il massimo degli assi. Una linea richiede almeno due punti, le barre almeno uno con valori X distinti. Il limite è **128 punti**.
- **Parti da una tabella.** Apri una tabella salvata e scegli **Grafico**. Seleziona due colonne numeriche diverse e, se serve, **Prima riga come intestazione**. Puoi usare la virgola o il punto decimale. I risultati numerici delle formule possono alimentare il grafico. Le linee richiedono almeno due coppie numeriche. Le barre richiedono almeno una coppia e valori X distinti. Le righe con entrambe le celle selezionate vuote vengono ignorate. Se l’anteprima segnala dati non validi, completa i valori mancanti o correggi le celle. Il grafico si ricalcola al salvataggio della tabella sorgente sulla stessa pagina.
- **Converti uno schizzo.** Disegna due assi che si incontrano in basso a sinistra e una curva o una serie di barre verticali. Seleziona lo schizzo con **Lazo** e scegli **Converti in grafico**. Rivedi i punti, imposta la scala se serve e salva sulla stessa pagina o su una nuova. [Da schizzo a grafico](GUIDA.md#da-schizzo-a-grafico)

Apri un grafico salvato dalla sezione **Grafici** per modificarlo. **Inserisci dalla libreria** copia i valori e le impostazioni nella pagina di destinazione. Se il grafico è collegato a una tabella, la copia usa i dati validi del momento e ha punti modificabili propri. Gli aggiornamenti alla tabella continuano ad agire sul grafico collegato originale.

**Copia CSV** e **Salva CSV** sono disponibili nell’editor del grafico indipendente dopo aver impostato la scala numerica. Il CSV usa i limiti calibrati degli assi e conserva anche i valori negativi quando il grafico è una copia di un grafico collegato a una tabella. Per importare i dati del grafico in una tabella, usa un CSV con al massimo 12 righe, intestazione inclusa.

## Tabelle

1. In **Tabelle** scegli **Nuova tabella**, dai un nome al quaderno e apri l’editor della griglia. Su una pagina, **Tabella** aggiunge invece l’elemento al quaderno corrente.
2. In una tabella nuova scegli **Importa CSV** e apri un file locale `.csv` o `.tsv`. Quaderno legge file UTF‑8 fino a **256 KiB** e riconosce virgole, punti e virgola o tabulazioni nei CSV. I campi tra virgolette possono contenere separatori, a capo e virgolette raddoppiate. La prima riga diventa modificabile come le altre. Quaderno completa le righe corte con celle vuote.
3. Seleziona una cella per modificarla. Usa **+** e **−** sotto **Righe** e **Colonne** per cambiare la griglia. **Tab** avanza, **Maiusc+Tab** torna alla cella precedente e **Invio** scende di una riga. Una tabella supporta fino a **12 righe × 8 colonne** e **500 caratteri per cella**.
4. Premi **Salva** per aggiungere la tabella all’elenco della libreria. **Esporta CSV** scrive la griglia corrente, comprese le modifiche della bozza aperta, in UTF‑8 con separatore virgola. Nelle celle con formule scrive il risultato o il codice di errore visualizzato. Per conservare le formule modificabili e i collegamenti ai grafici, trasferisci il quaderno.

Per calcolare un totale o una media, seleziona una cella vuota sotto i dati e premi **Somma** o **Media**. Il pulsante propone l’intervallo dalla prima riga fino alla riga precedente, nella colonna selezionata. Puoi modificare i riferimenti nel campo della cella. Usa riferimenti come `A1` o intervalli rettangolari come `A1:B3`. Per esempio, inserisci `=SOMMA(B2:B4)` in `B5` e `=MEDIA(B2:B4)` in `B6`. Puoi usare anche `SUM` e `AVERAGE`. Per più argomenti usa la virgola o il punto e virgola. Nei numeri scritti direttamente nella formula usa il punto decimale per evitare ambiguità.

Quaderno calcola anche le formule riconosciute nei CSV e TSV importati. La griglia e la pagina mostrano il risultato, mentre il campo della cella conserva la formula da modificare. Negli intervalli Quaderno ignora testo e celle vuote. Una media senza numeri mostra `#DIV/0!`, un riferimento fuori dalla tabella mostra `#REF!` e un riferimento circolare mostra `#CYCLE!`.

Sul Paper Pro scegli un file già presente sul tablet. Se il CSV è sul Mac, puoi importarlo lì e inviare il quaderno. Per riusare una tabella dentro Quaderno, scegli **Inserisci dalla libreria** nella pagina di destinazione. Ottieni una tabella separata che puoi modificare o usare come sorgente di un nuovo grafico.

## Come passano i contenuti tra le sezioni

| Da | A | Cosa succede |
| --- | --- | --- |
| Tabelle | Grafici | **Grafico** crea un collegamento a due colonne di una tabella sulla stessa pagina. Il grafico si ricalcola dopo il salvataggio delle modifiche. |
| Grafici o Tabelle | Scrittura o Libri | **Inserisci dalla libreria** aggiunge una copia indipendente e modificabile alla pagina di destinazione. |
| Scrittura o Libri | Grafici | Seleziona uno schizzo compatibile nella pagina e convertilo. Il grafico salvato compare in **Grafici** e lo schizzo resta nella pagina. |
| Grafici | Tabelle | Esporta i dati calibrati in CSV e importali in una nuova tabella. Il file conserva valori ed etichette delle colonne. |
| Scrittura | Libri | Esporta il quaderno come PDF e importalo in **Libri** per ottenere una copia da consultare che conserva l’aspetto delle pagine. |
| Qualsiasi quaderno | Un altro quaderno | Dalle miniature usa **Sposta in…**. Si sposta l’intera pagina con annotazioni e collegamenti tra tabella e grafico sulla stessa pagina. |

Per esempio, importa `vendite.csv` con le colonne **Mese** e **Vendite** in **Tabelle**. Usa mesi numerici come `1` e `2`. Salva e riapri la tabella, scegli **Grafico**, seleziona le due colonne e usa la prima riga come intestazione. Apri un report in **Scrittura** e inserisci una copia del grafico. Le modifiche successive alla tabella aggiornano il grafico originale. Per aggiornare i valori nel report, inserisci una nuova copia.

## Formati e trasferimenti

| Metodo | Cosa può usare chi riceve |
| --- | --- |
| Trasferimento del quaderno Quaderno | Mantiene modificabili sorgente Markdown, tratti e livelli, note, immagini incorporate, tabelle con formule, grafici e forme dei diagrammi. Sfondi, segnalibri e collegamenti tra tabella e grafico nella stessa pagina viaggiano con il quaderno. |
| Markdown con cartella degli allegati | Testo modificabile e file immagine collegati. Grafici e scrittura a mano sono immagini. |
| PDF | Copia da leggere o condividere con l’aspetto delle pagine, comprese annotazioni e oggetti visibili. |
| CSV di tabella o grafico | Valori delle celle, risultati delle formule o dati X/Y calibrati. Importa il CSV in **Tabelle** per riutilizzare i valori. |
| File draw.io | Forme, etichette e collegamenti compatibili da modificare in draw.io. |

Per passare da Mac a Paper Pro o viceversa, usa **Sincronizzazione wireless → Anteprima invio quaderno** e conferma l’invio. Il limite per quaderno preparato è **8 MiB** via Bluetooth LE e **64 MiB** via Wi‑Fi locale. Usa il Wi‑Fi per libri e quaderni più grandi. Dopo il primo invio, puoi attivare **Sincronizza automaticamente** e **Sincronizza questo quaderno**. Modifiche indipendenti possono produrre una copia di conflitto. Durante l’aggiornamento di un quaderno esistente, la destinazione conserva cartella, preferito, tag del quaderno e posizione di lettura locale. **Continua sul Mac** e **Continua sul Paper Pro** possono richiedere l’apertura di una pagina specifica. [Trasferimenti wireless e USB‑C](GUIDA.md#sincronizzazione-wireless)

Aggiorna entrambe le app prima di inviare quaderni con immagini incorporate, che usano il formato di libreria **8**. Per salvare o ripristinare l’intera libreria usa `quaderni.json`. [Dati e backup](DATI.md)
