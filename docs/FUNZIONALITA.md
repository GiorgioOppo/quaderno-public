# Sezioni di Quaderno e scambio dei file

[English](en/FEATURES.md) · **Italiano**

Quaderno ha cinque sezioni su Mac e Paper Pro. **Libri** e **Scrittura** aprono quaderni. **Grafici**, **Tabelle** e **Diagrammi** elencano gli elementi modificabili delle pagine. Diagrammi mantiene accessibile anche un quaderno vuoto prima del primo diagramma. Aprire una voce dell’indice mostra la pagina originale senza aprire un editor. Modificarla lì cambia lo stesso oggetto visibile nel quaderno sorgente; **Inserisci dalla libreria** crea invece una copia indipendente. Quinta sezione e nuovo riconoscimento fanno parte della 0.2.8 in sviluppo. Il 18 novembre 2026 l’utente ha confermato riconoscimento, applicazione e modifica nel diagramma provato sul Paper Pro; vedi l’[ambito delle verifiche](IMPOSTAZIONI.md#ambito-delle-verifiche).

| Sezione | Da dove partire | Cosa salvare o condividere |
| --- | --- | --- |
| [Libri](#libri) | PDF locale o EPUB senza DRM a impaginazione fluida | Quaderno con annotazioni modificabili, oppure PDF per leggere e condividere |
| [Scrittura](#scrittura) | Documento Markdown vuoto o file Markdown esistente | Quaderno modificabile, file Markdown con eventuali allegati, oppure PDF |
| [Grafici](#grafici) | Punti inseriti nell’editor, tabella sulla stessa pagina o schizzo a mano | Grafico modificabile, dati calibrati in CSV, oppure immagine del grafico nell’esportazione Markdown |
| [Tabelle](#tabelle) | Griglia vuota o file CSV o TSV in UTF‑8 | Tabella modificabile, CSV delle celle, oppure grafico collegato alla tabella |
| [Diagrammi](#diagrammi) | Quaderno vuoto, diagramma a mano libera, forme dell’editor o file draw.io supportato | Diagramma modificabile, PDF oppure PNG e `.drawio` nell’esportazione Markdown |

**File**, **Preferiti** e **Tag** filtrano la sezione aperta. Grafici, tabelle e diagrammi usano la cartella e lo stato di preferito del quaderno. Possono essere trovati con i tag del quaderno o della pagina. Il **Cestino** raccoglie i quaderni di tutte le sezioni. [Libreria e pagine](GUIDA.md#libreria-e-pagine)

## Libri

1. Scegli **Libri → Importa documento** e seleziona un PDF o un EPUB. Sul Mac, **Importa da Apple Books** offre due comandi. **Apri Apple Books** apre Libri. **Scegli PDF o EPUB…** seleziona copie locali, compresi gli EPUB che Finder mostra come un unico pacchetto. Se il PDF si apre in Anteprima, usa **File → Esporta** in Anteprima per salvarne una copia locale. La libreria iCloud e i titoli protetti restano accessibili in Apple Books. [PDF in Apple Books](https://support.apple.com/en-ge/guide/books/ibkseed72068/9.0/mac/27) · [Esportare da Anteprima](https://support.apple.com/guide/preview/export-pdfs-and-images-prvw0e8da223/mac)
2. Apri il libro importato. Usa **Evidenzia** per segnare un passaggio a mano libera. Scegli **Sottolinea** e trascina da un’estremità all’altra per tracciare una linea orizzontale. Usa **Aggiungi nota** per inserire un blocco di testo. Per modificare o spostare una nota salvata, attiva **Elementi** e tocca o trascina il blocco.
3. Usa lo zoom, **Adatta alla larghezza** e i segnalibri per consultare il libro. Quaderno ricorda l’ultima pagina su ogni dispositivo. **Cerca testo** usa il testo incorporato che il PDF rende estraibile. Per cercare in una scansione serve un livello di testo. Se hai importato il PDF con una build precedente, puoi importarlo di nuovo per indicizzarlo.
4. Scegli **Inserisci dalla libreria** per aggiungere una copia modificabile di un grafico, una tabella o un diagramma. Usa **Salva PDF** sul Paper Pro o **Condividi → Salva PDF** sul Mac per ottenere una copia annotata. Trasferisci il quaderno con Quaderno per continuare a modificare le annotazioni sull’altro dispositivo.

Quaderno conserva il PDF o EPUB originale e prepara i documenti in background, con avanzamento e annullamento. Renderizza soltanto le pagine richieste, alla risoluzione necessaria per lo zoom. L’importazione EPUB conserva testo, titoli, elenchi, tabelle e immagini locali supportate, anche quando racchiuse in SVG; usa un’impaginazione stabile senza riprodurre necessariamente tutti i font e gli stili originali. I precedenti limiti di 150 pagine e 30 MiB di immagini raster non si applicano più. Restano i limiti di **48 MiB** per il PDF e **64 MiB** per l’EPUB; il PDF preparato dall’EPUB deve rientrare in **48 MiB** e restano i limiti di dimensione della libreria e dei trasferimenti. Evidenziazioni, sottolineature e note restano annotazioni modificabili separate in Quaderno. Il PDF esportato combina gli elementi e i livelli visibili; testo PDF originale e oggetti Quaderno non restano elementi modificabili separati. [Leggere e annotare documenti](GUIDA.md#leggere-e-annotare-pdf-ed-epub)

## Scrittura

1. Scegli **Scrittura → Nuovo Markdown**. Inserisci un titolo e digita il sorgente o salva un documento vuoto. Per partire da un file locale, usa **Importa documento** con un `.md` o `.markdown`. Il pannello **Markdown e Obsidian** accetta anche un collegamento HTTPS diretto a un file Markdown. L’importazione accetta testo UTF‑8 fino a **256 KiB** e **200 pagine**.
2. Scegli **Modifica Markdown** per cambiare il sorgente. Quaderno mostra la sintassi Markdown come testo sulle pagine e ridistribuisce il contenuto quando lo salvi. Le annotazioni esistenti rimangono sulle proprie pagine. Se il sorgente si accorcia, una pagina con annotazioni può restare nel quaderno anche se il suo testo sorgente è stato spostato o rimosso.
3. Scrivi con il Marker sul Paper Pro o trascina il puntatore sul Mac. Puoi scegliere tra cinque modelli, tre spessori e livelli d’inchiostro. Il lazo sposta, ruota, ridimensiona o copia i tratti selezionati. **Testo** aggiunge un blocco modificabile separato dall’editor del sorgente. Sul Paper Pro con il motore installato, il lazo avvia la trascrizione in italiano. Su un Mac compatibile, scegli **Trascrivi in italiano**. [Trascrivere la scrittura a mano](GUIDA.md#da-scrittura-a-mano-a-testo)
4. Usa **Inserisci immagine** per un PNG o JPEG locale. Quaderno incorpora una copia. In **Elementi**, tocca l’immagine per ridimensionarla o eliminarla, oppure trascinala per spostarla. Il file di partenza può essere di **16 MiB**. La copia incorporata arriva fino a **1.200 pixel** sul lato più lungo e **768 KiB**. Ogni pagina può contenere fino a **12 immagini**.
5. Usa **Inserisci dalla libreria** per aggiungere copie modificabili di grafici, tabelle o diagrammi, oppure crea una tabella o un diagramma nella pagina. Nell’editor dei diagrammi, l’importazione draw.io legge il primo diagramma di un file `.drawio` o `.xml` compatibile. Esporta in `.drawio` per modificare forme e collegamenti supportati in draw.io. [Diagrammi di flusso](GUIDA.md#diagrammi-di-flusso)

**Markdown e Obsidian → Esporta Markdown** salva sorgente e blocchi di testo. Quando presenti, aggiunge collegamenti relativi alle immagini incorporate e ai PNG di grafici, diagrammi e inchiostro visibile in una cartella accanto al file. Ogni diagramma ha anche un allegato `.drawio` modificabile collegato dal Markdown. Sposta cartella e file insieme. Reimportare il Markdown recupera testo e collegamenti, non gli oggetti Quaderno originali modificabili. Esporta i valori delle tabelle separatamente in CSV; trasferisci un quaderno `.quaderno` o sincronizza il quaderno per conservare oggetti modificabili e tratti recuperabili. L’integrazione con i vault Obsidian scambia il testo Markdown. [Markdown e Obsidian](GUIDA.md#markdown-e-obsidian)

## Grafici

- **Inserisci punti a mano.** In **Grafici → Nuovo grafico**, dai un nome al quaderno e parti da due punti modificabili. Scegli linee o barre, modifica il titolo e le etichette degli assi, aggiungi, rimuovi o correggi i punti. L’editor usa coordinate da **0 a 100%**. **Scala numerica** imposta il massimo degli assi. Una linea richiede almeno due punti, le barre almeno uno con valori X distinti. Il limite è **128 punti**.
- **Parti da una tabella.** Apri una tabella salvata e scegli **Grafico**. Seleziona due colonne numeriche diverse e, se serve, **Prima riga come intestazione**. Puoi usare la virgola o il punto decimale. I risultati numerici delle formule possono alimentare il grafico. Le linee richiedono almeno due coppie numeriche. Le barre richiedono almeno una coppia e valori X distinti. Le righe con entrambe le celle selezionate vuote vengono ignorate. Se l’anteprima segnala dati non validi, completa i valori mancanti o correggi le celle. Il grafico si ricalcola al salvataggio della tabella sorgente sulla stessa pagina.
- **Converti uno schizzo.** Disegna due assi che si incontrano in basso a sinistra e una curva o una serie di barre verticali. Seleziona lo schizzo con **Lazo** e scegli **Converti in grafico**. Rivedi i punti, imposta la scala se serve e salva sulla stessa pagina o su una nuova. [Da schizzo a grafico](GUIDA.md#da-schizzo-a-grafico)

Aprire un grafico salvato dalla sezione **Grafici** mostra la sua pagina. Tocca il grafico con il dito, il mouse o un breve contatto della penna per modificarlo; **Elementi** resta disponibile per modificare e spostare. **Inserisci dalla libreria** copia i valori e le impostazioni nella pagina di destinazione. Se il grafico è collegato a una tabella, la copia usa i dati validi del momento e ha punti modificabili propri. Gli aggiornamenti alla tabella continuano ad agire sul grafico collegato originale.

**Copia CSV** e **Salva CSV** sono disponibili nell’editor del grafico indipendente dopo aver impostato la scala numerica. Il CSV usa i limiti calibrati degli assi e conserva anche i valori negativi quando il grafico è una copia di un grafico collegato a una tabella. Per importare i dati del grafico in una tabella, usa un CSV con al massimo 12 righe, intestazione inclusa.

## Tabelle

1. In **Tabelle** scegli **Nuova tabella**, dai un nome al quaderno e apri l’editor della griglia. Su una pagina, **Tabella** aggiunge invece l’elemento al quaderno corrente.
2. In una tabella nuova scegli **Importa CSV** e apri un file locale `.csv` o `.tsv`. Quaderno legge file UTF‑8 fino a **256 KiB** e riconosce virgole, punti e virgola o tabulazioni nei CSV. I campi tra virgolette possono contenere separatori, a capo e virgolette raddoppiate. La prima riga diventa modificabile come le altre. Quaderno completa le righe corte con celle vuote.
3. Seleziona una cella per modificarla. Usa **+** e **−** sotto **Righe** e **Colonne** per cambiare la griglia. **Tab** avanza, **Maiusc+Tab** torna alla cella precedente e **Invio** scende di una riga. Una tabella supporta fino a **12 righe × 8 colonne** e **500 caratteri per cella**.
4. Premi **Salva** per aggiungere la tabella all’elenco della libreria. **Esporta CSV** scrive la griglia corrente, comprese le modifiche della bozza aperta, in UTF‑8 con separatore virgola. Nelle celle con formule scrive il risultato o il codice di errore visualizzato. Per conservare le formule modificabili e i collegamenti ai grafici, trasferisci il quaderno.

Per calcolare un totale o una media, seleziona una cella vuota sotto i dati e premi **Somma** o **Media**. Il pulsante propone l’intervallo dalla prima riga fino alla riga precedente, nella colonna selezionata. Puoi modificare i riferimenti nel campo della cella. Usa riferimenti come `A1` o intervalli rettangolari come `A1:B3`. Per esempio, inserisci `=SOMMA(B2:B4)` in `B5` e `=MEDIA(B2:B4)` in `B6`. Puoi usare anche `SUM` e `AVERAGE`. Per più argomenti usa la virgola o il punto e virgola. Nei numeri scritti direttamente nella formula usa il punto decimale per evitare ambiguità.

Sono disponibili anche `MIN`, `MAX` e `COUNT`/`CONTA.NUMERI`, con pulsanti rapidi e una guida alle formule. Le intestazioni della griglia mostrano lettere e numeri; gli errori della cella selezionata sono spiegati sotto la formula. Ridurre una tabella con celle compilate richiede conferma. Aggiorna entrambe le app alla 0.2.1 per calcolare le nuove funzioni.

Quaderno calcola anche le formule riconosciute nei CSV e TSV importati. La griglia e la pagina mostrano il risultato, mentre il campo della cella conserva la formula da modificare. Negli intervalli Quaderno ignora testo e celle vuote. Una media senza numeri mostra `#DIV/0!`, un riferimento fuori dalla tabella mostra `#REF!` e un riferimento circolare mostra `#CYCLE!`.

Sul Paper Pro scegli un file già presente sul tablet. Se il CSV è sul Mac, puoi importarlo lì e inviare il quaderno. Per riusare una tabella dentro Quaderno, scegli **Inserisci dalla libreria** nella pagina di destinazione. Ottieni una tabella separata che puoi modificare o usare come sorgente di un nuovo grafico.

## Diagrammi

1. Scegli **Diagrammi → Nuovo diagramma** e dai un nome al quaderno. Si apre una pagina vuota per disegnare, senza forme provvisorie o apertura automatica dell’editor. Il quaderno rimane nella sezione anche vuoto o con sola scrittura a mano.
2. Disegna rettangoli di processo, rombi di decisione, ovali di inizio/fine e frecce di collegamento. Nei quaderni Diagrammi, il riconoscimento automatico può proporre un suggerimento dopo una pausa. Aprilo per rivederlo; **Chiudi** lo nasconde fino a una successiva modifica al disegno. Puoi anche selezionare i tratti con il lazo e richiedere il riconoscimento in un’altra pagina.
3. Rivedi forme e frecce, poi trascrivi o digita le etichette. Escludi un’etichetta per lasciare intatti i suoi tratti. Applica il diagramma scegliendo se sostituire i tratti convertiti o conservare tutto l’inchiostro. I tratti ambigui restano. La sostituzione conserva anche gli originali rimossi per recuperarli in seguito, separatamente dall’annullamento di sessione.
4. Tocca un diagramma salvato per aprirne l’editor, oppure usa **Elementi**. Modifica etichette, forme, collegamenti e disposizione; importa file draw.io supportati o esporta `.drawio`. **Inserisci dalla libreria** copia un diagramma salvato in un altro quaderno come oggetto modificabile indipendente.

Il riconoscimento geometrico è locale; quello del testo usa il motore disponibile sul dispositivo. Non richiede un nuovo account cloud. Il modello supporta 64 nodi e 128 collegamenti diretti. Vedi [Diagrammi](DIAGRAMMI.md) per consigli sul disegno, limiti e recupero.

## Come passano i contenuti tra le sezioni

| Da | A | Cosa succede |
| --- | --- | --- |
| Tabelle | Grafici | **Grafico** crea un collegamento a due colonne di una tabella sulla stessa pagina. Il grafico si ricalcola dopo il salvataggio delle modifiche. |
| Grafici, Tabelle o Diagrammi | Scrittura o Libri | **Inserisci dalla libreria** aggiunge una copia indipendente e modificabile alla pagina di destinazione. |
| Scrittura o Libri | Grafici | Seleziona uno schizzo compatibile nella pagina e convertilo. Il grafico salvato compare in **Grafici** e lo schizzo resta nella pagina. |
| Pagina con inchiostro | Diagrammi | Rivedi una conversione con il lazo; l’oggetto salvato compare nell’indice Diagrammi e resta sulla pagina originale. |
| Diagrammi | Pagine con grafici o tabelle | Inserisci un diagramma modificabile indipendente accanto agli altri elementi. |
| Grafici | Tabelle | Esporta i dati calibrati in CSV e importali in una nuova tabella. Il file conserva valori ed etichette delle colonne. |
| Scrittura | Libri | Esporta il quaderno come PDF e importalo in **Libri** per ottenere una copia da consultare che conserva l’aspetto delle pagine. |
| Qualsiasi quaderno | Un altro quaderno | Dalle miniature usa **Sposta in…**. Si sposta l’intera pagina con annotazioni e collegamenti tra tabella e grafico sulla stessa pagina. |

Per esempio, importa `vendite.csv` con le colonne **Mese** e **Vendite** in **Tabelle**. Usa mesi numerici come `1` e `2`. Salva e riapri la tabella, scegli **Grafico**, seleziona le due colonne e usa la prima riga come intestazione. Apri un report in **Scrittura** e inserisci una copia del grafico. Le modifiche successive alla tabella aggiornano il grafico originale. Per aggiornare i valori nel report, inserisci una nuova copia.

## Formati e trasferimenti

| Metodo | Cosa può usare chi riceve |
| --- | --- |
| Trasferimento del quaderno Quaderno | Mantiene modificabili sorgente Markdown, tratti e livelli, note, immagini incorporate, tabelle con formule, grafici e forme dei diagrammi. Sfondi, segnalibri e collegamenti tra tabella e grafico nella stessa pagina viaggiano con il quaderno. |
| Markdown con cartella degli allegati | Testo modificabile e file immagine collegati. Grafici e scrittura a mano sono immagini; i diagrammi hanno sia anteprime PNG sia allegati `.drawio` modificabili. |
| PDF | Copia da leggere o condividere con l’aspetto delle pagine, comprese annotazioni e oggetti visibili. |
| CSV di tabella o grafico | Valori delle celle, risultati delle formule o dati X/Y calibrati. Importa il CSV in **Tabelle** per riutilizzare i valori. |
| File draw.io | Forme, etichette e collegamenti compatibili da modificare in draw.io. |

Per passare da Mac a Paper Pro o viceversa, usa **Sincronizzazione wireless → Anteprima invio quaderno** e conferma l’invio. Con entrambe le app aggiornate, il limite per quaderno preparato è **64 MiB** via Bluetooth LE o Wi‑Fi locale. Si trasferiscono solo pagine e risorse mancanti, con ripresa delle parti già salvate dopo un’interruzione. Il Wi‑Fi è preferibile per il primo invio di libri grandi. Un’app precedente usa ancora il trasferimento completo, fino a 8 MiB via Bluetooth; le nuove proprietà richiedono entrambe le app aggiornate. Dopo il primo invio, puoi attivare **Sincronizza automaticamente** e **Sincronizza questo quaderno**. I controlli della 0.2.9 in sviluppo aggiungono scelta automatica Wi-Fi/Bluetooth e sincronizzazione dopo le modifiche; vedi [sincronizzazione automatica](PROTOCOLLO_SYNC.md#sincronizzazione-automatica-nella-029) per requisiti e stato delle verifiche. Modifiche indipendenti possono produrre una copia di conflitto. Durante l’aggiornamento di un quaderno esistente, la destinazione conserva cartella, preferito, tag del quaderno e posizione di lettura locale. **Continua sul Mac** e **Continua sul Paper Pro** possono richiedere l’apertura di una pagina specifica. [Trasferimenti wireless e USB‑C](GUIDA.md#sincronizzazione-wireless)

Aggiorna entrambe le app prima di trasferire documenti con sorgenti native, che usano il formato portabile **9**. Il file locale `quaderni.json` è ora un manifest: un backup completo richiede anche la directory `.objects` e i file di recupero e posizione di lettura. Usa l’esportazione portabile di Quaderno per ottenere un file di trasferimento autonomo. [Dati e backup](DATI.md)
