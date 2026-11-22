# Guida a Quaderno

[English](en/USER_GUIDE.md) · **Italiano**

Quaderno è una libreria locale per scrivere a mano, organizzare quaderni e annotare documenti. Per preparare il Mac o il Paper Pro, segui la [guida all’installazione](INSTALLAZIONE.md). Per trasferire o mettere al sicuro la libreria, consulta [Dati e backup](DATI.md).

Le [sezioni di Quaderno e lo scambio dei file](FUNZIONALITA.md) descrivono cosa puoi creare in Libri, Scrittura, Grafici, Tabelle e Diagrammi e come riusare i contenuti tra le sezioni.

In **Impostazioni → Lingua e tastiera**, scegli **Italiano** o **English** sotto **Lingua dell’app**. Quaderno ricorda la scelta alla riapertura. La [guida alle impostazioni 0.2.6](IMPOSTAZIONI.md) descrive la navigazione aggiornata e lo stato delle verifiche.

## Il primo quaderno

1. In **Scrittura** scegli **Nuovo Markdown**, inserisci un nome e salva. Puoi già digitare il testo o iniziare con una pagina vuota.
2. Apri **Modello** e scegli **Bianco**, **Righe**, **Quadretti**, **Puntini** o **Planner**. Il modello si applica alla pagina aperta. Le nuove pagine vuote ereditano quello corrente.
3. Scrivi con il Marker sul Paper Pro. Su Mac, fai clic e trascina nella pagina. Il pulsante con il nome della penna apre **Strumenti di scrittura**.
4. Scegli **Nuova pagina** per continuare oppure **Libreria** per tornare ai quaderni. Quaderno salva le modifiche mentre lavori.

### Usare il Marker sul Paper Pro

- Tocca con la punta del Marker per premere i pulsanti, aprire quaderni, cambiare pagina, usare le impostazioni o digitare sul tastierino del PIN.
- Sulla pagina del quaderno, scrivi con la punta usando lo strumento attivo. Usa l’estremità con la gomma, se presente, per cancellare i tratti.
- Nell’editor dei diagrammi, usa la punta per selezionare, spostare o ridimensionare le forme e per collegarle.

## Libreria e pagine

![Libreria di Quaderno con i comandi di importazione, cartelle e filtri](immagini/libreria.png)

Scegli una sezione della libreria per raggiungere il contenuto che ti serve.

- **Libri** raccoglie i PDF e gli EPUB importati.
- **Scrittura** raccoglie i quaderni con testo Markdown e annotazioni a mano libera.
- **Diagrammi** elenca diagrammi modificabili e quaderni Diagrammi vuoti. Scegli **Nuovo diagramma** per aprire una pagina da disegnare; aprire una voce esistente mostra la pagina sorgente senza aprire automaticamente l’editor. Tocca un diagramma salvato per modificarlo, oppure usa **Elementi**. La quinta sezione fa parte della 0.2.8 in sviluppo; vedi [Diagrammi](DIAGRAMMI.md).
- **Grafici** e **Tabelle** raccolgono gli elementi modificabili già salvati nelle pagine. Scegli un elemento per aprire la sua pagina. La correzione in sviluppo nella **0.2.4** evita l’apertura automatica dell’editor durante questa navigazione: per modificare una tabella tocca una cella; per un grafico attiva **Elementi** e toccalo. La 0.2.4 ha superato compilazione, sei suite e avvii renderizzati su Mac e con l’SDK ARM. È stata installata sul Paper Pro tramite USB il 18 novembre 2026, con avvio nativo, backup e integrità dei quaderni verificati; l’app Mac 0.2.0 è rimasta invariata. Lo stesso giorno l’utente ha confermato sul Paper Pro che aprire elementi esistenti da Tabelle e Grafici mostra il foglio senza aprire l’editor. Consulta lo [stato della verifica](LASSO.md#aprire-tabelle-e-grafici-dalla-libreria). Puoi anche creare un quaderno per un nuovo grafico o una nuova tabella dalla relativa sezione.

### Cartelle, preferiti, tag e cestino

- In **File**, usa **Nuova cartella** per creare una cartella nella posizione corrente. Apri una cartella per creare al suo interno un quaderno o una sottocartella. Il menu **…** della cartella consente di rinominarla e di eliminarla quando è vuota.
- Il menu **…** accanto a un quaderno contiene **Aggiungi ai preferiti**, **Sposta in una cartella**, **Gestisci tag**, **Duplica quaderno** e **Sposta nel cestino**.
- Usa **Preferiti** e **Tag** per filtrare i quaderni. La ricerca della libreria trova titoli, tag dei quaderni e tag delle pagine. In **File**, cerca anche nelle sottocartelle. Con **Ordina** puoi passare dall’ordine della libreria all’ordinamento alfabetico.
- Nel **Cestino** puoi ripristinare un quaderno oppure eliminarlo definitivamente dopo conferma.

### Panoramica delle pagine

Apri il contatore in basso, per esempio **2 / 5**, per vedere le miniature. Seleziona una pagina, poi usa **Apri**, **Duplica**, **Tag pagina**, **Sposta in…** o **Elimina**. Le frecce **Sposta indietro** e **Sposta avanti** cambiano l’ordine all’interno del quaderno. **Sposta in…** trasferisce la pagina selezionata in un altro quaderno.

Quando riapri un quaderno, Quaderno torna all’ultima pagina che hai consultato su quel dispositivo. La posizione segue la pagina quando la riordini.

Puoi assegnare fino a 20 tag per pagina o quaderno, con un massimo di 40 caratteri per tag. Dentro un quaderno, **Cerca testo** cerca nei blocchi digitati, nelle celle delle tabelle, nelle etichette dei grafici, nei tag di pagina, nelle trascrizioni salvate e nel testo estratto dai PDF. Su Mac puoi usare **⌘F** per aprire la ricerca.

## Scrittura e contenuti della pagina

- **Modifica Markdown** apre il testo del documento. Puoi usare la tastiera sul Mac o quella a schermo sul Paper Pro.
- **Strumenti di scrittura** offre penna, matita, pennarello, evidenziatore e gomma per tratti interi. Sono disponibili tre spessori. Con **Linea**, **Rettangolo** o **Ellisse** puoi disegnare una forma trascinando tra i suoi estremi.
- **Inserisci immagine** apre un PNG o JPEG locale. La copia incorporata si apre anche sui dispositivi che ricevono il quaderno.
- **Inserisci dalla libreria** copia un grafico o una tabella nella pagina corrente. Puoi modificare la copia indipendentemente dall’originale.
- **Testo** inserisce un blocco su più righe. Tocca un blocco esistente per aprirlo e selezionare o modificare il testo. Sul Paper Pro, quando il sistema rileva una tastiera Bluetooth o USB collegata, la tastiera a schermo si nasconde.
- Per scegliere il layout QWERTY della tastiera a schermo, tocca **IT/EN** oppure apri **Impostazioni → Lingua e tastiera → Tastiera a schermo**. Puoi passare da **Italiano** a **English (US)** anche mentre scrivi. Quaderno ricorda il layout separatamente dalla lingua dei menu.
- **Tabella** inserisce fino a 12 righe e 8 colonne. Nell’editor, tocca una cella nella griglia, modifica il campo **Testo della cella** e premi **Salva**. Con la tastiera, **Tab** passa alla cella successiva e **Invio** scende di una riga.
- **Elementi** attiva la modifica di blocchi, tabelle e grafici. Tocca un elemento per aprirlo o eliminarlo dal suo editor. Trascinalo per spostarlo sulla pagina. Premi **Scrivi** per tornare alla penna.
- **Pagina intera** nasconde gli strumenti. Usa **Mostra strumenti** per ripristinare la barra.

Sul Paper Pro con firmware **3.29.1.0**, Quaderno usa la modalità rapida del pannello per l’interfaccia e mantiene quella standard per le pagine con documenti, immagini, grafici o diagrammi. **Aggiornamento automatico** chiede un ridisegno solo se la nuova pagina tarda a comparire. Puoi disattivarlo in **Impostazioni → Schermo**. Per pulire eventuali immagini residue su tutto il pannello, tocca **Aggiorna schermo** nella barra in basso, anche in **Pagina intera**. Le impostazioni 0.2.6 espongono il comando manuale anche in **Generali** e **Schermo**. La pulizia completa può far lampeggiare lo schermo.

### Riconoscere una pagina mista sul Paper Pro

In Quaderno 0.2.17 puoi ricavare un diagramma di flusso, una tabella e del testo da una pagina e salvarli in un unico quaderno. L’analisi e il riconoscimento della scrittura avvengono sul tablet, anche offline.

1. Apri il foglio scritto a mano in **Scrittura** o la pagina del PDF importato in **Libri**. Scegli **Altro → Riconosci pagina mista**.
2. Seleziona **Diagramma**, **Tabella** o **Testo**, poi disegna con il Marker un contorno attorno al contenuto nell’anteprima. Quaderno chiude il contorno quando sollevi la penna. Usa più contorni per una parte distribuita sulla pagina. Dopo aver indicato tutte e tre le parti, scegli **Riconosci**.
3. Confronta le bozze con l’anteprima della pagina di partenza. Controlla forme, frecce ed etichette e correggile nell’editor del diagramma. Verifica righe e celle della tabella. Inserisci ogni riga della tabella su una nuova riga di testo e separa le colonne con Tab o `|`. Correggi o completa la trascrizione nella scheda **Testo**.
4. Inserisci un titolo e scegli **Crea quaderno**. Quaderno apre il risultato in **Scrittura**. Le prime pagine contengono il Markdown, seguito da una pagina per il diagramma e una per la tabella, entrambi modificabili. La prima pagina contiene i due collegamenti. La pagina di partenza resta disponibile con i suoi contenuti originali.

Su un foglio scritto a mano Quaderno può usare i tratti della penna per riconoscere le forme. Su un PDF importato analizza l’immagine della pagina, dove scrittura e frecce possono essere più difficili da distinguere. La bozza può saltare forme, collegamenti, parole o separazioni tra colonne. Puoi completarla durante la revisione, anche quando un motore di riconoscimento locale è indisponibile.

Con il motore OCR opzionale della 0.2.18, l’area **Testo** usa un modello locale per l’alfabeto latino su PDF e immagini. Durante la revisione confronta la bozza con la pagina di partenza, soprattutto per indirizzi web, percorsi di file, nomi e punteggiatura. [Installare il motore OCR opzionale](INSTALLAZIONE.md#installare-e-avviare)

Dalla 0.2.19 lo stesso motore legge l’area **Tabella**. Se la tabella ha una griglia, disegna un unico contorno che racchiuda anche i bordi esterni. Quando riconosce chiaramente la griglia, Quaderno propone i valori nelle rispettive celle, lasciando spazio a quelle vuote. Per tabelle a colonna singola, senza bordi o con una griglia incerta, controlla le righe proposte e separa le colonne con Tab o `|`. Usa due separatori consecutivi per una cella vuota tra due valori. Controlla la posizione di righe e colonne, soprattutto dove la scrittura attraversa un divisorio. Quaderno salva una tabella modificabile fino a 12 righe e 8 colonne.

Dalla 0.2.19 il motore propone anche le etichette delle forme riconosciute nei diagrammi in PDF e immagini. Confronta le etichette con la pagina di partenza e modificale insieme a forme e frecce prima di salvare.

### Aprire gli elementi collegati da Scrittura

Scegli **Elementi collegati** per vedere l’anteprima del diagramma o della tabella. **Apri** mostra la pagina originale e **Modifica** avvia l’editor dell’oggetto. Puoi anche toccare un richiamo per andare direttamente alla pagina originale. Usa **Torna alla pagina** per rientrare nel testo. Per aggiungere manualmente un riferimento a un oggetto già presente, scegli **Collega elemento**.

Usa Quaderno 0.2.17 o una versione successiva su entrambi i dispositivi per ritrovare gli stessi comandi di modifica. Una sola sincronizzazione o esportazione del quaderno convertito porta con sé Markdown, diagramma, tabella e collegamenti. Per un collegamento a un altro quaderno, sincronizza la destinazione separatamente. Vale anche per i risultati salvati in tre quaderni con la 0.2.16. [Sincronizzare i quaderni](#sincronizzare-i-quaderni)

### Modificare una cella sul foglio

Nella 0.2.2, attiva **Elementi**, tocca la tabella e scegli la cella nella griglia dell’editor. La correzione in sviluppo per la **0.2.3** permette di aprire direttamente la cella toccandola sul foglio: dito e mouse funzionano anche sulle celle vuote; un tocco breve della penna/Marker apre una cella compilata. Nelle celle vuote la penna continua a scrivere, anche con singoli punti. Muovere o tenere appoggiata la penna continua la scrittura anche nelle celle compilate. In Elementi, il tocco apre l’editor sulla riga e colonna scelte.

Modifica il campo **Testo della cella**, poi premi **Salva**. Per le formule, il campo mostra l’espressione originale modificabile e la griglia mostra il risultato. La conversione della scrittura con il lazo produce le stesse celle modificabili. La 0.2.3 ha superato compilazione, sei suite di test e prove di avvio su Mac e con l’SDK ARM. È stata installata tramite USB e avviata sul Paper Pro il 18 novembre 2026, con backup e integrità dei quaderni verificati; l’app Mac 0.2.0 è rimasta invariata. Lo stesso giorno l’utente ha confermato l’apertura con dito e penna e la corretta modifica con ricalcolo delle formule nel caso di prova **QA Celle 0.2.2**. Per la procedura completa, i valori della prova e i limiti del collaudo, consulta la [guida al lazo](LASSO.md#modificare-una-cella-dopo-la-conversione).

### Creare una tabella da un CSV

Sulla pagina in cui vuoi inserire la tabella, scegli **Tabella → Importa CSV** e apri un file `.csv` o `.tsv` locale. Sul Mac puoi partire anche dalla sezione **Tabelle** della libreria e creare un quaderno per la nuova tabella. Controlla le celle nell’editor, correggile se necessario e premi **Salva**.

Quaderno legge file in UTF‑8 con colonne separate da virgole, punti e virgola o tabulazioni. Puoi importare fino a 12 righe e 8 colonne, con 500 caratteri al massimo per cella. La prima riga viene importata come tutte le altre.

Sul Paper Pro puoi scegliere un file già presente sul tablet. Se il CSV è sul Mac, puoi importarlo in Quaderno e poi [inviare il quaderno al Paper Pro](#inviare-un-quaderno-dal-mac-al-paper-pro).

Per esportare i dati, apri la tabella e scegli **Esporta CSV**. Il file include i risultati delle formule.

### Calcolare somma e media

Seleziona una cella vuota sotto i valori e premi **Somma** o **Media** per proporre un intervallo nella stessa colonna. Puoi scrivere o modificare la formula nel campo della cella. Per esempio, usa `=SOMMA(B2:B4)` in `B5` e `=MEDIA(B2:B4)` in `B6`. Funzionano anche `SUM` e `AVERAGE`. La griglia mostra il risultato o un codice di errore.

### Creare un grafico da una tabella

Per creare un grafico indipendente, apri **Grafici → Nuovo grafico** nella libreria. Puoi scegliere linee o barre, modificare i due punti iniziali e impostare il titolo e le etichette degli assi.

1. Crea e salva una tabella con almeno due colonne. Tocca direttamente una cella con il dito per aprire l’editor; puoi anche usare **Elementi** e toccare la tabella.
2. Premi **Grafico**. Il comando salva prima la bozza corrente della tabella, poi apre le impostazioni del grafico. **Annulla** nel dialogo del grafico non annulla questo salvataggio della tabella.
3. Scegli **Linee** o **Barre** e due colonne numeriche distinte per X e Y. Per i decimali puoi usare la virgola o il punto. Per un grafico a linee servono almeno due righe di dati. Per le barre, usa un valore X distinto per ciascuna riga.
4. Lascia attiva **Prima riga come intestazione** soltanto se la prima riga contiene i titoli delle colonne, da usare come etichette degli assi. L’opzione è attiva inizialmente: disattivala se la prima riga contiene già dati numerici, così quei valori vengono inclusi.
5. Controlla l’anteprima e premi **Salva** per aggiungere il grafico collegato alla stessa pagina.

Il grafico usa anche i risultati numerici delle formule e si aggiorna quando salvi una modifica alla tabella. Se le colonne selezionate contengono valori incompleti o non validi, correggi le celle per ripristinare il grafico. Una copia indipendente del grafico esporta anche i valori negativi con **Copia CSV** o **Salva CSV**.

### Modificare un grafico con un tocco

Nella **0.2.5** in sviluppo puoi aprire l’editor toccando direttamente il grafico sul foglio con il dito, con un clic del mouse o con un tocco breve della penna/Marker. Un grafico indipendente apre le proprie proprietà; un grafico collegato a una tabella apre le impostazioni e la scelta delle colonne sorgente, mantenendo il collegamento quando salvi.

Il tocco diretto è previsto con penna, matita o pennarello selezionati e il lazo chiuso. Muovere o tenere appoggiata la penna continua la scrittura. Lazo, gomma, evidenziatore e strumenti per le forme conservano il proprio comportamento. Puoi ancora attivare **Elementi** per modificare o trascinare gli oggetti. Aprire una voce dalla libreria Grafici continua a mostrare soltanto il foglio, come nella 0.2.4.

La 0.2.5 ha superato compilazione, tutte le sei suite e avvii renderizzati su Mac e con l’SDK ARM, comprese otto nuove prove dei grafici e le regressioni della navigazione 0.2.4. È stata installata sul Paper Pro tramite USB il 18 novembre 2026, con avvio nativo, esecuzione tramite AppLoad, backup e integrità dei quaderni verificati; l’app Mac 0.2.0 è rimasta invariata. Lo stesso giorno l’utente ha confermato sul Paper Pro l’apertura dell’editor del grafico provato con dito e tocco breve della penna, senza attivare Elementi. Consulta la [guida ai gesti e lo stato delle verifiche](LASSO.md#modificare-un-grafico-con-un-tocco).

### Diagrammi di flusso

Nella 0.2.8 **Diagrammi → Nuovo diagramma** crea un quaderno con una pagina vuota da disegnare. Traccia un diagramma di flusso con forme e punte delle frecce ben distinguibili. Con il riconoscimento automatico attivo, una pausa può produrre un suggerimento senza cambiare la pagina o aprire una finestra. Aprilo per rivederlo; **Chiudi** lo nasconde fino a una successiva modifica del disegno. In alternativa seleziona i tratti con il lazo e richiedi il riconoscimento, anche nelle altre sezioni.

Rivedi forme, frecce ed etichette, correggi o escludi le trascrizioni e applica esplicitamente. Puoi conservare tutto l’inchiostro oppure sostituire soltanto forme/collegamenti riconosciuti ed etichette accettate non vuote. Tratti ambigui ed esclusi restano. L’annullamento della pagina ripristina l’intera conversione; quando l’inchiostro è sostituito, il recupero degli originali resta disponibile dopo la riapertura. **Inserisci dalla libreria** crea una copia modificabile indipendente in un altro quaderno. Vedi [Diagrammi](DIAGRAMMI.md) per riconoscimento e recupero in dettaglio. La prima build 0.2.8 ha superato controlli automatici/nativi e la rotazione automatica da verticale a orizzontale è stata confermata sul tablet. Le tolleranze riviste del riconoscimento hanno superato verifiche geometriche/UI e avvii Mac e ARM; la build corretta è installata sul Paper Pro e ha superato l’avvio rigoroso nativo/AppLoad a 1620 × 2160 senza errori QML. Il 18 novembre 2026 l’utente ha confermato che riconoscimento, applicazione del risultato e apertura dell’editor per modificarlo funzionano nel diagramma di prova sul Paper Pro; vedi lo [stato attuale delle verifiche](IMPOSTAZIONI.md#ambito-delle-verifiche).

Resta disponibile l’editor strutturato:

1. Apri una pagina e scegli **Diagramma**. Aggiungi le forme che ti servono oppure usa **Modello di flusso** per partire da un esempio modificabile. Da un diagramma vuoto, **Importa .drawio** apre anche i file `.xml` di draw.io. Quaderno importa la prima pagina e segnala gli elementi che ha omesso o semplificato.
2. Trascina le forme per disporle e usa il quadratino nell’angolo per ridimensionarle. **Griglia** allinea lo spostamento e il ridimensionamento ai punti della griglia. Seleziona una forma per duplicarla o per modificare la sua etichetta.
3. Con **Collega**, tocca la forma di partenza e poi quella di arrivo. Le frecce seguono le forme quando le sposti. Seleziona una freccia per aggiungere un’etichetta, invertirla o passare al percorso ad angolo retto. Puoi annullare e ripristinare le modifiche fatte nell’editor.
4. Salva il diagramma nel quaderno. Per riaprirlo, tocca direttamente il diagramma salvato oppure scegli **Elementi** e toccalo. L’esportazione PDF include il disegno. Dal menu **Altre azioni → Esporta .drawio** puoi salvare forme e frecce modificabili in draw.io.

Aggiorna Quaderno su Mac e Paper Pro per trasferire i diagrammi mantenendoli modificabili. Ogni diagramma può contenere fino a 64 forme e 128 collegamenti. L’importazione accetta file fino a 2 MiB.

Per sistemare più forme insieme, attiva **Lazo** e cerchia i loro centri. Trascina una delle forme selezionate per spostarle tutte. In **Allinea e distribuisci** puoi allineare due o più forme e, se c’è spazio sufficiente, lasciare lo stesso intervallo tra tre o più forme. Puoi annullare ogni spostamento o allineamento in un solo passaggio.

Usa **Ingrandisci vista** e **Riduci vista** per cambiare lo zoom. Con **Sposta vista** puoi trascinare il diagramma per raggiungere un’altra zona. Disattiva il comando per tornare a modificare le forme, oppure usa **Adatta alla vista** per rivedere tutto il diagramma.

### Livelli di inchiostro

Apri **Livelli** per aggiungere un livello o sceglierne uno esistente. La penna accanto al nome indica il livello attivo. Usa **Nascondi** o **Mostra** per controllarne la visibilità. Se il livello attivo è nascosto, rendilo visibile oppure selezionane un altro prima di scrivere.

Puoi rinominare, riordinare, unire o eliminare livelli. **Unisci sotto** richiede la stessa visibilità per i due livelli. Ogni pagina può avere fino a 12 livelli e deve conservarne almeno uno. Gomma, lazo e cronologia **Annulla** e **Ripeti** operano sui tratti del livello attivo. Nell’esportazione PDF compaiono i livelli visibili.

### Forme intelligenti e ripristino

1. Apri **Strumenti di scrittura**, attiva **Forme intelligenti** e scegli penna, matita o pennarello.
2. Disegna una linea, un rettangolo allineato alla pagina o un’ellisse. Tieni ferma la punta alla fine del tratto.
3. Quando compare la forma in anteprima, solleva la punta per confermarla. Sposta la punta per riprendere il tratto libero.
4. Per recuperare il tratto a mano, attiva **Lazo**, circonda la forma e scegli **Ripristina tratti originali**. Il ripristino è reversibile con **Annulla**.

## Selezionare e modificare i tratti

Premi **Lazo** e disegna un contorno attorno ai tratti del livello attivo. Al rilascio si apre il pannello **Tratti selezionati**.

### Spostare, ruotare e copiare i tratti

- Scegli **Sposta e trasforma** e trascina la selezione nell’anteprima con il dito sul Paper Pro o con il puntatore sul Mac. Usa **−**, **+** o i pulsanti di rotazione per modificarla. Premi **Applica** per salvare. Una selezione che contiene rettangoli o ellissi corretti con **Forme intelligenti** ruota a passi di 90° per conservare i tratti originali.
- Scegli **Copia tratti**, apri la pagina di destinazione e premi **Incolla** nella barra degli strumenti. Posiziona la copia nell’anteprima e premi **Incolla** per aggiungerla al livello attivo.
- Usa **Annulla** e **Ripeti** sulla pagina in cui hai applicato lo spostamento o l’incolla.

### Da schizzo a grafico

1. Circonda i due assi del grafico e una singola curva o una serie di barre verticali. Gli assi devono incontrarsi in basso a sinistra. Lascia fuori le scritte a mano e le serie sovrapposte.
2. Scegli **Converti in grafico**. Controlla l’anteprima e compila il titolo e le etichette degli assi.
3. Usa **Modifica punti** per correggere le coordinate percentuali di un punto. Premi **Applica punto** per confermare la modifica.
4. Per ottenere valori numerici, attiva **Scala numerica** e inserisci il valore all’estremità di ciascun asse, partendo da zero. Se l’asse X va da 0 a 12 mesi e l’asse Y da 0 a 200 euro, inserisci rispettivamente **12** e **200**. A questo punto puoi usare **Copia CSV** o **Salva CSV**.
5. Premi **Salva**. Per tenere il grafico separato dallo schizzo, seleziona **Salva su una nuova pagina**. Il grafico è vettoriale e lo schizzo originale resta nel quaderno.

![Anteprima di un grafico convertito con campi per titolo, assi e scala numerica](immagini/conversione-grafico.png)

### Da scrittura a mano a testo

Sul Paper Pro con il motore di trascrizione installato, cerchia una frase con **Lazo**. Quando sollevi la penna, Quaderno avvia il riconoscimento in italiano e mostra un’anteprima in **Tratti selezionati**. Sul Mac, seleziona la frase e premi **Trascrivi in italiano**. [Installazione](INSTALLAZIONE.md)

Apri **Rivedi la trascrizione** per correggere il testo. Scegli **Sostituisci sulla pagina** per inserire il testo nella posizione dei tratti trascritti. Per modificarlo in seguito, tocca il blocco con lo strumento **Penna** attivo oppure selezionalo da **Elementi**. Usa **Annulla** per ripristinare la scrittura a mano. Per conservare entrambe le versioni, scegli **Salva in una nuova pagina**. In questo caso **Cerca testo** può riportarti alla pagina manoscritta originale. Il riconoscimento avviene sul dispositivo e la precisione varia con la grafia.

## Documenti, importazione e condivisione

### Leggere e annotare PDF ed EPUB

In libreria seleziona **Importa documento**, poi scegli un PDF locale o un EPUB senza DRM a impaginazione fluida. Apri il volume da **Libri**. Quaderno usa le pagine come sfondo per le annotazioni. Gli EPUB vengono reimpaginati dal testo, con supporto per titoli, enfasi ed elenchi. Immagini, font incorporati e impaginazione editoriale restano fuori dalla copia.

Nella barra di lettura scegli **Evidenzia** e trascina sulla pagina per segnare un passaggio. Con **Sottolinea**, trascina da un’estremità all’altra per tracciare una linea orizzontale. Puoi scegliere colore e spessore dagli strumenti di scrittura. Con **Aggiungi nota**, digita e salva un blocco di testo sulla pagina. Per riaprire la nota e modificarla, scegli **Elementi** e tocca il blocco.

- Usa **+** e **−** per ingrandire o ridurre la pagina e **Adatta alla larghezza** per occupare tutta la larghezza disponibile. Tocca la percentuale per tornare alla pagina intera.
- Tocca la stella per aggiungere o rimuovere il segnalibro della pagina corrente. Apri **Segnalibri** per tornare direttamente a una pagina salvata.
- Scegli **Cerca testo** in fondo alla pagina per trovare parole nel testo incorporato del PDF. Seleziona un risultato per aprirne la pagina. La ricerca usa il testo che il PDF rende estraibile. Per cercare in un PDF importato con una versione precedente di Quaderno, importa nuovamente il file originale.

L’importazione lavora in background con avanzamento e annullamento, conserva la sorgente nativa e renderizza soltanto le pagine richieste. Non c’è più il limite di 150 pagine. La sorgente può occupare fino a 48 MiB per il PDF e 64 MiB per l’EPUB; il PDF preparato dall’EPUB deve rientrare in 48 MiB. Restano i limiti di capacità della libreria e di trasferimento. Le vecchie importazioni raster conservano le annotazioni; reimporta l’originale come copia separata per usare il nuovo lettore. Scegli **Salva PDF** sul Paper Pro o **Condividi → Salva PDF** sul Mac per esportare tutte le pagine, con i livelli visibili e gli oggetti della pagina.

Tocca due volte con un dito per ingrandire al 200% intorno al punto toccato; ripeti per adattare l’intera pagina. La penna continua ad annotare. Apri il contatore delle pagine per vedere **Pagine**, inserisci un numero e scegli **Vai**, poi **Apri**. **Prima** e **Ultima** selezionano gli estremi del documento. Filtra la panoramica per **Tutte**, **Segnalibri** o **Annotate**; i conteggi includono annotazioni nei livelli nascosti. Il tastierino numerico compare sul Paper Pro quando non è collegata una tastiera fisica.

Nella libreria, **Ordina** offre A–Z, Z–A e ordine per numero di pagine, oltre ai filtri per formato. Le vecchie importazioni raster sono indicate come documenti importati in precedenza senza presumere che siano PDF o EPUB. Sul Mac, trascinare fino a 16 file mostra destinazione e conteggi dei file accettati, duplicati, non supportati e non leggibili. **Importazioni** apre il rapporto per file. **Riprova non riusciti** riprova soltanto i file falliti; **Annulla importazione** interrompe il lavoro in attesa conservando le importazioni completate. Il rapporto riguarda l’ultimo gruppo e si cancella al blocco di Quaderno o con il comando dedicato.

### Markdown e Obsidian

- In libreria, usa **Importa documento** e scegli un file `.md` o `.markdown`. Su Mac puoi anche trascinarlo dal Finder o scegliere **Apri con → Quaderno**. Quaderno conserva la sintassi Markdown e suddivide il testo in pagine. Per modificare il testo del documento, scegli **Modifica Markdown**.
- Su Mac e Paper Pro, apri **Markdown e Obsidian**. Per importare dal web, incolla l’URL HTTPS diretto al file, per esempio il collegamento **Raw** di un repository, e premi **Importa da URL**. L’importazione accetta testo UTF-8 fino a 256 KiB e 200 pagine.
- Dal quaderno aperto, scegli **Markdown e Obsidian → Esporta Markdown**. Quaderno salva inchiostro, grafici, diagrammi e immagini incorporate come file separati in una cartella accanto al Markdown, con collegamenti relativi. Ogni diagramma ha un’anteprima PNG e un allegato `.drawio` modificabile. Reimportare il Markdown recupera testo e collegamenti; il trasferimento del quaderno conserva gli oggetti originali modificabili. Per conservare i collegamenti, sposta insieme il file e la cartella. L’integrazione con Obsidian invia il testo Markdown.
- Su Mac, scegli la cartella del **Vault Obsidian** e inserisci il nome del file o un percorso relativo, per esempio `Riunioni/Verbale.md`. La sottocartella deve già esistere. **Salva nel vault** crea una nuova nota e rifiuta un nome già in uso. Dopo il salvataggio, usa **Apri in Obsidian**. [Obsidian URI](https://obsidian.md/help/uri)
- Per un sito già pubblicato, salva l’indirizzo HTTPS in **Obsidian Publish** e, se vuoi collegare una nota specifica, inserisci il suo percorso pubblicato o il permalink. **Copia collegamento** è disponibile su entrambi i dispositivi e **Apri nel browser** su Mac. La pubblicazione delle note si gestisce in Obsidian. [Obsidian Publish](https://obsidian.md/help/publish) · [Permalink](https://obsidian.md/help/publish/permalinks)

### Obsidian Sync sul Paper Pro

Obsidian Sync collega direttamente il Paper Pro al tuo vault remoto. Dopo la prima [configurazione via USB‑C](INSTALLAZIONE.md#collegare-obsidian-sync), bastano Quaderno aperto e sbloccato e una rete Wi‑Fi con accesso a Internet sul tablet.

1. In Quaderno apri **Markdown e Obsidian → Obsidian Sync** e scegli **Sincronizza ora**. Le note del vault vengono scaricate sul tablet. Seleziona una nota per aprire il quaderno collegato. Le modifiche dal cloud si applicano quando riapri la nota da questo pannello.
2. Per aggiornare la nota in Obsidian, modifica il testo da **Elementi** e salva. Torna al pannello **Obsidian Sync** e scegli **Invia modifiche** per quella nota. Il testo Markdown viene inviato al vault. Inchiostro, tabelle, grafici e altri elementi del quaderno restano in Quaderno.
3. Se Quaderno rileva una modifica incompatibile, conserva il quaderno collegato. Scegli **Importa copia dal cloud** per aprire il testo remoto in un quaderno separato. Anche i quaderni collegati a note rimosse o rinominate nel vault rimangono nella libreria di Quaderno.

Il pannello elenca fino a 3.000 note `.md` in UTF‑8, con un limite di 256 KiB e 200 pagine per nota importata. Le note scaricate occupano spazio sul tablet. Puoi consultare i quaderni già aperti anche offline. La sincronizzazione si avvia dal pannello mentre Quaderno è in uso.

### Portare i quaderni dal Paper Pro al Mac

1. Collega il Paper Pro in modalità sviluppatore al Mac con un cavo USB‑C dati e sblocca il tablet. Al primo collegamento verifica l’identità del tablet in Terminale con `ssh root@10.11.99.1`.
2. Nella libreria Mac scegli **Importa dal Paper Pro**. Inserisci le credenziali SSH del tablet nel pannello protetto se vengono richieste.
3. Controlla quanti quaderni saranno creati, aggiornati o lasciati invariati e quante copie in conflitto saranno aggiunte. L’anteprima indica anche le eventuali cartelle nuove. Scegli **Importa quaderni** per applicare l’anteprima o **Conferma** se non ci sono cambiamenti visibili. Se la libreria del Mac cambia con l’anteprima aperta, rivedila prima di confermare.
4. Apri il quaderno importato. Ripeti l’importazione quando il quaderno sul tablet cambia. Se hai modificato anche il contenuto della copia Mac, Quaderno conserva entrambe le versioni.

**Importa quaderni** apre un’esportazione portabile di Quaderno copiata sul Mac, fino a 64 MiB. Il solo manifest locale incrementale `quaderni.json` non è una libreria portabile: conserva anche directory `.objects` e file accessori per un backup completo, oppure esporta attraverso Quaderno. I quaderni nativi reMarkable e reMarkable Connect usano un archivio separato. Consulta [Dati e backup](DATI.md).

### Inviare un quaderno dal Mac al Paper Pro

1. Installa sul Paper Pro una versione di Quaderno che supporta il trasferimento USB‑C. Usa lo stesso cavo e la stessa configurazione SSH dell’importazione dal tablet. Consulta [Installazione](INSTALLAZIONE.md).
2. Apri il quaderno sul Mac e scegli **Invia al Paper Pro**. L’anteprima indica l’origine, la destinazione e l’esito previsto dell’invio. Quando le due copie differiscono, indica anche quante pagine presentano modifiche.
3. Chiudi Quaderno sul Paper Pro e scegli **Invia quaderno** sul Mac. Se il contenuto coincide, scegli **Conferma**. Riapri Quaderno sul tablet per vedere il risultato.

Ogni invio trasferisce un quaderno fino a 64 MiB. L’aggiornamento conserva la cartella, lo stato di preferito e le etichette presenti sul Paper Pro. Se le copie differiscono e Quaderno non può verificare che quella sul tablet coincida con l’ultima versione condivisa, la conserva e ne aggiunge una separata. Se il quaderno selezionato sul Mac o la libreria del Paper Pro cambia dopo l’anteprima, rivedi il trasferimento prima di inviarlo.

### Sincronizzazione wireless

Aggiorna Quaderno su entrambi i dispositivi e tieni le app aperte e sbloccate.

#### Collegare via Bluetooth

1. Sul Paper Pro vai in **Impostazioni → Bluetooth**, tocca **Apri sincronizzazione Bluetooth** e scegli **Avvia Bluetooth**. Sul firmware 3.29.1.0, Quaderno attiva il controller se necessario e mostra il codice per il primo abbinamento.
2. Sul Mac apri **Impostazioni → Bluetooth → Apri sincronizzazione Bluetooth**. Inserisci il codice mostrato sul Paper Pro e scegli **Collega il Paper Pro**. Consenti l’accesso a Bluetooth se macOS lo richiede.

L’abbinamento collega le due app Quaderno. Dopo un riavvio del Paper Pro, usa **Avvia Bluetooth** per riattivare il collegamento.

#### Collegare via Wi-Fi locale

1. Collega entrambi i dispositivi alla stessa rete Wi-Fi. Sul Paper Pro apri **Sincronizzazione wireless** con l’icona delle due frecce, scegli **Wi-Fi locale** e lascia attivo il collegamento.
2. Sul Mac apri lo stesso pannello. Inserisci il codice e l’indirizzo IPv4 mostrati dal Paper Pro, poi scegli **Wi-Fi locale**. Consenti l’accesso alla rete locale se macOS lo richiede.

#### Sincronizzare i quaderni

1. Apri un quaderno sul dispositivo da cui vuoi inviarlo. Scegli **Anteprima invio quaderno**, controlla l’azione e premi **Conferma invio**. Con entrambe le app aggiornate, il limite è 64 MiB per quaderno esportato su entrambi i collegamenti. Quaderno invia solo le pagine e le risorse mancanti, in parti da 64 KiB confermate dopo il salvataggio. Con un’app precedente, il trasferimento completo conserva il limite di 8 MiB via Bluetooth e 64 MiB via Wi-Fi; le nuove proprietà richiedono entrambe le app aggiornate.
2. Per inviare automaticamente le modifiche successive, attiva **Sincronizza automaticamente** e **Sincronizza questo quaderno** sui dispositivi da cui vuoi inviare i quaderni selezionati. Scegli **Dopo le modifiche**, **All’apertura di un quaderno** oppure un intervallo. Per il passaggio automatico, aggiorna entrambe le app alla 0.2.13 e imposta **Automatico (Bluetooth e Wi-Fi)** su entrambi i dispositivi. La modalità predefinita resta manuale. Il primo trasferimento richiede revisione; i conflitti seguono **Chiedi cosa fare** o **Conserva entrambe le copie**, senza sovrascrivere una copia modificata indipendentemente. Le eliminazioni restano locali.

Tieni entrambe le app Quaderno aperte e sbloccate. La modalità dopo le modifiche attende una pausa nella scrittura. Attivazione dell’automatismo e riconnessione ricontrollano i quaderni selezionati. Il collegamento automatico usa il Bluetooth già acceso per rilevare le modifiche pendenti. Il Mac usa l’IPv4 Wi-Fi attuale comunicato dal Paper Pro per provare il trasferimento sulla rete locale. Se il Wi-Fi è indisponibile, il trasferimento continua via Bluetooth. Gli SSID attendibili limitano il Wi-Fi e il vincolo di ricarica vale per entrambi i collegamenti. **Disconnetti** sospende la riconnessione fino a **Riprendi collegamento** o a un collegamento manuale. **Rimuovi abbinamento** cancella il codice salvato e le preferenze di sincronizzazione per quel dispositivo. [Recupero, limiti e verifiche della sincronizzazione automatica](PROTOCOLLO_SYNC.md#sincronizzazione-automatica).

Per continuare sull’altro dispositivo, apri la pagina e scegli **Continua sul Mac** o **Continua sul Paper Pro**. Se serve trasferire la versione corrente, Quaderno mostra prima l’anteprima. Sull’altro dispositivo scegli **Mostra richiesta → Apri pagina**. Se il quaderno è cambiato dopo l’invio o è in conflitto, verifica le due versioni e invia una nuova richiesta.

La libreria e l’editor mostrano lo stato dei quaderni già condivisi. **Ultima versione condivisa** indica che la copia locale coincide con la revisione confermata dall’altro dispositivo. **Modifiche da sincronizzare** indica che la copia locale è cambiata. Apri **Sincronizzazione wireless** per leggere i dettagli di un conflitto o errore e la data dell’ultimo scambio riuscito.

Puoi scollegare il cavo dopo aver avviato Quaderno. Per riaprire l’app dal menu del tablet, usa la voce **Quaderno** di AppLoad se configurata. La sessione USB temporanea scade dopo 10 minuti. [Avvio dal menu AppLoad](INSTALLAZIONE.md#avviare-dal-menu-appload)

Se il collegamento si interrompe, le parti ricevute restano sul dispositivo. Il lavoro automatico interrotto prima dell’applicazione può riprovare con attese limitate e una nuova anteprima; resta disponibile anche **Riprendi trasferimento**. La conferma precedente non viene riutilizzata e un esito di applicazione incerto richiede revisione. Le versioni in conflitto restano separate e le formule delle tabelle rimangono modificabili.

### Note, Libri e iCloud sul Mac

Per importare un libro, scegli **Importa da Apple Books** dalla libreria di Quaderno sul Mac. **Apri Apple Books** apre Libri. Torna in Quaderno e scegli **Scegli PDF o EPUB…** per selezionare una copia locale senza DRM. Se apri un PDF di Libri in Anteprima, puoi salvarne una copia con **File → Esporta**. Puoi selezionare anche i pacchetti `.epub` che Finder mostra come un singolo libro. L’accesso alla libreria nel cloud e ai titoli protetti del Book Store resta in Apple Books. [PDF in Libri](https://support.apple.com/it-it/guide/books/ibkseed72068/mac) · [Esportare un PDF da Anteprima](https://support.apple.com/it-it/guide/preview/prvw0e8da223/mac)

Dopo l’importazione apri il libro in Quaderno. Da **Sincronizzazione wireless**, scegli **Anteprima invio quaderno**, controlla la destinazione e conferma l’invio. Il limite riguarda il quaderno preparato per il trasferimento, che comprende sorgenti native, eventuali vecchie immagini delle pagine e annotazioni. Le app aggiornate trasferiscono fino a 64 MiB anche via Bluetooth, riprendendo le parti mancanti dopo un’interruzione. Il Wi-Fi resta preferibile per il primo invio di libri grandi. Con un’app precedente, Bluetooth mantiene il limite di 8 MiB. [Sincronizzazione wireless](#sincronizzazione-wireless)

Trascina un PDF o EPUB senza DRM dal Finder alla finestra di Quaderno per importarlo. Puoi anche scegliere **Apri con → Quaderno** dal Finder. Dal quaderno aperto scegli **Condividi**, poi trascina **Pagina corrente** o **Intero quaderno** in una cartella del Finder per ottenere un PDF.

- Per allegare il quaderno in **Note**, usa **Condividi → Condividi PDF** e seleziona **Note** nel menu di macOS, se disponibile. Puoi scegliere una nota nuova o esistente. Puoi anche salvare il PDF e condividerlo da Finder. [Condividere un file in Note](https://support.apple.com/it-it/102223)
- Per aggiungerlo a **Libri**, scegli **Condividi → Salva PDF**. In Libri apri **File → Importa** e seleziona il PDF. [Importare PDF in Libri](https://support.apple.com/en-am/guide/books/ibkseed72068/mac)
- Per conservare una copia su **iCloud Drive**, scegli quella cartella nel pannello di salvataggio. La sincronizzazione delle copie in Note o Libri dipende dalle impostazioni iCloud di Apple. Le modifiche alle copie si fanno nelle rispettive app. [Note con iCloud](https://support.apple.com/it-it/guide/icloud/-mm2d069f7097/icloud) · [Libri con iCloud](https://support.apple.com/it-it/guide/icloud/mm3941ae3362/icloud)

## Impostazioni

Apri **Impostazioni** dalla libreria. La versione 0.2.6 in sviluppo aggiunge categorie con ricerca; consulta [Impostazioni, schermo e servizi collegati](IMPOSTAZIONI.md) per valori iniziali, disponibilità per piattaforma e stato delle verifiche.

- **Generali** riassume dispositivo e batterie, offre collegamenti alle categorie e, sul Paper Pro, refresh, sospensione e luce di lettura quando disponibili.
- **Wi-Fi** sul Paper Pro cerca le reti e usa il servizio del sistema per collegarsi a reti aperte o WPA/WPA2 personali. Quaderno passa la password al servizio per la connessione. Sul Mac il Wi-Fi viene gestito dal sistema.
- **Bluetooth** apre l’[abbinamento tra le due app](#collegare-via-bluetooth). Sul Paper Pro con firmware **3.29.1.0** gestisce anche gli [accessori nelle vicinanze](#collegare-una-tastiera-bluetooth).
- **Schermo** controlla orientamento manuale del tablet, dimensione, indicatori di stato, ingresso a pagina intera, gesture dei libri e refresh automatico/manuale. La 0.2.8 aggiunge l’orientamento automatico opzionale con sensore supportato. È disattivato inizialmente e attende un angolo stabile e la conclusione delle interazioni. Un orientamento di lettura fisso del libro ha precedenza; scegliere un angolo manuale disattiva la rotazione automatica. Vedi [impostazioni di rotazione](IMPOSTAZIONI.md#rotazione-lettura-e-schermo-intero).
- **Penna e batteria** mostra letture separate per tablet e penna, intervallo di sospensione e luce di lettura della sessione. Restare attivi durante la ricarica non disabilita il timeout del PIN.
- **Account e servizi** apre le integrazioni esistenti e identifica i servizi gestiti sul Mac. Aprire un browser o importare un file non crea un collegamento all’account cloud in Quaderno.
- **Lingua e tastiera** separa la lingua dell’interfaccia dal layout QWERTY italiano o inglese USA.
- In **USB‑C**, premi **Verifica** per controllare se il servizio SSH risponde a `10.11.99.1:22`. Sul Mac trovi anche **Copia comando** e, quando disponibile, **Apri in Terminale**.
- In **Sicurezza**, **PIN di Quaderno** imposta un PIN da 4 a 8 cifre. Puoi cambiarlo, rimuoverlo oppure usare **Blocca ora**. Con il PIN attivo, Quaderno si blocca all’apertura e dopo 1, 5 o 15 minuti di inattività. Il valore iniziale è 5 minuti. Il sistema operativo gestisce lo sblocco del dispositivo e l’accesso ai file.
- **Informazioni** mostra i dati del dispositivo e del software. Batteria e luce di lettura si trovano in **Penna e batteria**.

### Collegare una tastiera Bluetooth

1. Metti la tastiera in modalità di abbinamento.
2. Sul Paper Pro, apri **Impostazioni → Bluetooth**, attiva il Bluetooth e tocca **Cerca dispositivi**.
3. Accanto alla tastiera, scegli **Abbina**. Se Quaderno mostra un codice da digitare sulla tastiera, scrivilo sulla tastiera fisica e premi **Invio**. Per gli altri tipi di codice, segui le indicazioni sullo schermo.
4. Quando la tastiera è collegata, apri un campo di testo, un documento Markdown o una cella per scrivere. Nelle impostazioni Bluetooth puoi anche riconnettere, disconnettere o dissociare gli accessori.

Il Paper Pro usa i profili Bluetooth supportati dal firmware. Le tastiere Bluetooth con profilo HID usano l’input del sistema.
