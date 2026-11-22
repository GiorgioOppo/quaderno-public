# Cronologia delle modifiche

[English](CHANGELOG.md) · **Italiano**

## 3.0.0

- Condividi lo schermo del Paper Pro via Wi-Fi locale. La finestra su Mac segue le proporzioni e la rotazione del tablet. Dal tablet puoi concedere o revocare il controllo tramite clic e tastiera sulla diretta.
- Apri gli strumenti della sezione in modalità a pagina intera. Nei libri usa scorrimento e zoom e apri note e collegamenti nel punto in cui li hai inseriti.
- Scrivi con il Marker anche partendo dal pulsante degli strumenti a pagina intera. Il trascinamento disegna sul foglio e un tocco breve continua ad aprire gli strumenti.
- Rivedi testo manoscritto, valori delle celle e grafici a mano libera prima di convertirli in elementi modificabili. I segni non riconosciuti possono restare sul foglio.

Vedi la [guida al collaudo funzionale](docs/COLLAUDO_FUNZIONALE.md) e lo [stato del rilascio e dell’installazione](docs/RILASCIO.md).

## 0.2.24 in sviluppo

- Invia dal Paper Pro la pagina corrente o l’intero quaderno come PDF annotato via Wi-Fi locale o Bluetooth. Il Mac abbinato salva il PDF nei Download e mostra la conferma di ricezione.
- Invia il PDF come allegato dal Paper Pro tramite un server SMTP configurato in **Impostazioni → Account e servizi**. La connessione richiede TLS con verifica del certificato.
- Passa tra le pagine Scrittura, Tabella, Grafico e Diagramma dallo stesso quaderno, usando le schede. Le sezioni della libreria aprono gli stessi elementi modificabili sulle pagine originali.

## 0.2.23 in sviluppo

- Aggiunge una palette UML modificabile con classi, interfacce, oggetti, attori, casi d’uso, componenti, pacchetti, nodi di distribuzione e linee di vita. Le forme si possono scambiare tramite file draw.io. I quaderni con forme UML usano il formato portabile 14.
- Migliora il riconoscimento delle etichette manoscritte brevi vicino alle forme piccole nei diagrammi di PDF e immagini. Adatta il testo delle etichette alle forme quando mostra il diagramma sul foglio.

## 0.2.22 in sviluppo

- Riunisce condivisione ed esportazione su Paper Pro e Mac. Il menu propone PDF annotato per una pagina o tutto il quaderno, PNG della pagina corrente, Markdown con allegati facoltativi, draw.io modificabile, CSV, il file nativo Quaderno e il PDF o EPUB originale quando conservato.
- Usa le finestre native di macOS per condividere e stampare. La condivisione del Markdown con allegati crea uno ZIP. Sul Paper Pro apri la sincronizzazione wireless per trasferire il quaderno al Mac e stamparlo oppure salva un PDF annotato.

## 0.2.21 in sviluppo

- Migliora il riconoscimento delle etichette piccole, ruotate o con lettere in colonna nei diagrammi. Per i tratti del Marker puoi riprovare una singola etichetta scegliendo l’orientamento. Per PDF e immagini, il motore OCR opzionale aggiornato valuta anche le lettere in colonna nei ritagli verticali.

## 0.2.20 in sviluppo

- Migliora l’associazione delle etichette manoscritte alle forme. Rivedi e correggi i testi proposti prima di applicare il diagramma.
- Aggiunge database, sottoprocesso, schermo, input manuale, preparazione, attesa, connettore fuori pagina e nuvola alla palette modificabile e allo scambio con i file draw.io compatibili. I quaderni nativi con queste forme richiedono il formato portabile 13. Quelli con le forme della palette originale usano la versione minima richiesta dai contenuti.

Vedi [Diagrammi](docs/DIAGRAMMI.md).

## 0.2.19 in sviluppo

- Disegna con il Marker un contorno a mano libera attorno al diagramma, alla tabella o al testo di una pagina mista. Quaderno chiude il contorno quando sollevi la penna.
- Estende l’OCR locale opzionale all’area **Tabella** di una pagina mista in PDF o con un’immagine come sfondo sul Paper Pro. Con una griglia ben visibile propone celle modificabili e conserva la posizione di quelle vuote. Se la griglia è incerta, puoi rivedere le righe proposte e aggiungere i separatori delle colonne.
- Il motore OCR locale opzionale propone etichette modificabili per le forme riconosciute nei diagrammi in PDF e immagini.

Superate tutte le 22 suite CTest e le prove di avvio nei layout desktop e tablet su macOS e con l’SDK ARM64 del Paper Pro. Il collaudo sul Paper Pro con firmware 3.29.1.0 ha coperto l’avvio a schermo intero da AppLoad e i contorni con il Marker. Una prova isolata ha usato l’OCR e salvato un quaderno collegato alle pagine modificabili della tabella e del diagramma. Controlla le bozze OCR, in particolare caratteri simili, etichette e posizioni delle celle. [Guida d’uso](docs/GUIDA.md#riconoscere-una-pagina-mista-sul-paper-pro) · [Stato del rilascio](docs/RILASCIO.md)

## 0.2.18 in sviluppo

- Aggiunge un motore OCR locale opzionale per l’area **Testo** del riconoscimento delle pagine miste sul Paper Pro. Legge testo in alfabeto latino da pagine PDF e immagini. Confronta la bozza con l’originale, soprattutto per indirizzi web, percorsi, nomi e punteggiatura.

Vedi [Riconoscere una pagina mista sul Paper Pro](docs/GUIDA.md#riconoscere-una-pagina-mista-sul-paper-pro).

## 0.2.17 in sviluppo

- Salva il risultato del riconoscimento in un unico quaderno modificabile in Scrittura. La prima pagina rimanda al diagramma e alla tabella su pagine dedicate dello stesso quaderno. Un file Quaderno o un’operazione di sincronizzazione trasferisce il testo e i due oggetti modificabili.
- La duplicazione e la copia conservata in caso di conflitto mantengono i riferimenti alle proprie pagine.
- Migliora il riconoscimento di riquadri annidati e frecce nei PDF manoscritti. I bordi della tabella producono meno righe spurie. Le trascrizioni e le etichette restano verificabili nella bozza.

Vedi [Riconoscere una pagina mista sul Paper Pro](docs/GUIDA.md#riconoscere-una-pagina-mista-sul-paper-pro).

## 0.2.16 in sviluppo

- Sul Paper Pro puoi selezionare le aree di diagramma, tabella e testo su un foglio o in un PDF importato. Rivedi le bozze elaborate sul tablet e salva insieme tre quaderni modificabili. La pagina di partenza resta disponibile.
- In Scrittura puoi vedere un’anteprima degli elementi collegati, aprirli e modificarli. Per usare i collegamenti sull’altro dispositivo, invia separatamente il quaderno di Scrittura e quelli delle destinazioni.

Vedi [Riconoscere una pagina mista sul Paper Pro](docs/GUIDA.md#riconoscere-una-pagina-mista-sul-paper-pro).

## 0.2.15 in sviluppo

- Aggiunge le impostazioni per l’accesso SSH sul Paper Pro con firmware 3.29.1.0. Il Wi-Fi usa l’impostazione persistente di reMarkable. Il controllo USB-C blocca o consente nuove connessioni fino al prossimo riavvio. La voce Bluetooth indica l’assenza di un listener SSH dedicato nel firmware.

Vedi [Accesso SSH sul Paper Pro](docs/IMPOSTAZIONI.md#accesso-ssh-sul-paper-pro).

## 0.2.14 in sviluppo

- Introduce un menu per Libri, Scrittura, Grafici, Tabelle e Diagrammi. Libri si apre in modalità lettura, con evidenziazioni, sottolineature, Post-it e collegamenti agli oggetti della libreria. Scrittura tiene a portata di mano le impostazioni della penna e il lazo.
- I collegamenti ancorati aprono il grafico, la tabella o il diagramma originale e offrono un comando per tornare alla pagina di partenza. Richiedono il formato portabile 12. Trasferisci a parte gli eventuali quaderni che contengono le destinazioni.
- La creazione di un grafico da tabella permette di scegliere le colonne X e Y. Una tabella sulla stessa pagina resta collegata. Una tabella su un’altra pagina fornisce una copia modificabile dei valori calcolati. Il menu Tabelle raccoglie formule, CSV, conversione della scrittura e creazione dei grafici.
- L’editor Diagrammi ha una palette, il trascinamento delle forme, i connettori e un ispettore. Il menu della sezione raccoglie riconoscimento a mano libera, importazione ed esportazione draw.io.

Vedi [le cinque sezioni e i loro flussi](docs/FUNZIONI_E_FLUSSI.md#le-cinque-sezioni).

## 0.2.13 in sviluppo

- Avvia la sincronizzazione automatica sul Bluetooth LE abbinato. Controlla le modifiche pendenti prima di provare l’indirizzo Wi-Fi locale attuale del Paper Pro. Trasferisce i dati sul Wi-Fi quando il collegamento è autenticato e usa il Bluetooth se il Wi-Fi manca o cade.
- Conserva la coda, le parti salvate e il controllo dei conflitti al cambio di collegamento. Un’applicazione interrotta con esito incerto richiede una nuova revisione. Il passaggio sui dispositivi fisici resta da collaudare.

Vedi il [protocollo di sincronizzazione automatica](docs/PROTOCOLLO_SYNC.md#sincronizzazione-automatica).

## 0.2.12 in sviluppo

- Aggiunge uno schema bilingue delle cinque sezioni, dei formati, dei collegamenti tra funzioni e delle integrazioni disponibili sui due dispositivi.
- Tiene in vista le azioni più frequenti della sezione. **Altro** raccoglie le restanti. La scelta dell’oggetto compare quando la pagina ne contiene più di uno adatto. Nelle finestre più strette i comandi di lettura si dispongono su due righe.
- Apre prima il selettore per CSV e draw.io. L’editor compare dopo aver scelto il file. Se annulli torni al foglio. Se nel frattempo cambi pagina devi selezionare di nuovo il file.
- Aggiunge zoom e spostamento a due dita sui fogli, anche in orizzontale. Tocca la percentuale per riadattare il foglio. Sul Mac sono accettati gli eventi di pinch del trackpad. Le preferenze di lettura si applicano ai libri e un secondo dito interrompe lo scorrimento senza cambiare pagina.

Le cinque suite interessate e le prove di avvio sono superate sulle build finali Mac e ARM. Le prove coprono finestra da 380 px al 130%, layout orizzontale, navigazione da tastiera, importazione CSV e draw.io, zoom e spostamento. Resta la prova fisica delle gesture.

Vedi lo [schema di funzioni e flussi](docs/FUNZIONI_E_FLUSSI.md).

## 0.2.11 in sviluppo

- Aggiunge **Post-it ancorati a un punto della pagina di un libro**. Puoi combinare testo modificabile da tastiera e scrittura a mano, scegliere il colore, riaprire il richiamo numerato, spostarlo o eliminarlo. Il salvataggio controlla libro, pagina e nota originali per impedire che una bozza obsoleta sovrascriva contenuti modificati. Creazione, modifica, spostamento ed eliminazione supportano Annulla/Ripeti della pagina; il testo digitato partecipa alla ricerca.
- Conserva note leggibili nelle esportazioni: il PDF dispone i richiami numerati sulle pagine originali e aggiunge appendici con testo impaginato e pagine separate per la scrittura; il Markdown con allegati include testo e PNG dei tratti. Esportazione nativa e sincronizzazione di Quaderno conservano testo e singoli tratti modificabili.
- Assegna a **Libri, Scrittura, Grafici, Tabelle e Diagrammi** una propria barra contestuale. Libri offre annotazioni e sincronizzazione del libro intero; Scrittura espone Markdown/testo/immagini/contenuti della libreria; Grafici propone dati e grafici da tabella; Tabelle offre celle, formule e CSV; Diagrammi espone riconoscimento, forme e importazione `.drawio`. Le azioni aprono direttamente l’unico elemento compatibile oppure chiedono quale usare se ce ne sono più di uno.
- Rende esplicito il trasferimento delle annotazioni dell’intero libro e mostra lo stato di sincronizzazione in Libri. Evidenziazioni e sottolineature appartengono già ai normali contenuti di pagina; i trasferimenti conservano tutte le pagine e i livelli nascosti, oltre alle nuove note. Documenta i requisiti: app aperte e sbloccate, stesso libro nativo di Quaderno e modalità di sincronizzazione appropriata.
- Introduce il formato portabile **11** per le note di pagina e metadati di versione 11 per i nuovi salvataggi locali. Le esportazioni portabili senza contenuti nuovi continuano a usare il formato minimo necessario. La negoziazione wireless comunica il formato supportato e rifiuta contenuti incompatibili prima di inviare le parti; per i destinatari precedenti con protocollo incrementale ma senza capacità esplicita usa il formato 9 come limite. Aggiorna entrambe le app per le nuove note.
- Recupera ovali e rettangoli disegnati a mano con piccole aperture o lati irregolari. Riconosce punte esplicite disegnate insieme a una linea retta o a gomito e punte separate con un breve tratto iniziale. Le linee senza punta o ambigue restano a penna.
- **Verifiche:** superate la suite macOS e le prove ARM64 interessate. L’avvio sul tablet da AppLoad ha conservato la libreria. La riproduzione dei tratti ha verificato il recupero di altre forme e punte di freccia esplicite.

Vedi [note sui libri](docs/NOTE_LIBRI.md), [flussi per sezione](docs/FUNZIONALITA.md), [sincronizzazione](docs/PROTOCOLLO_SYNC.md) e [compatibilità dei dati](docs/DATI.md#versioni-della-libreria).

## 0.2.10 in sviluppo

- Riconosce le aggiunte a mano libera nel contesto di un diagramma di flusso già convertito. L’anteprima contiene l’intero diagramma aggiornato, con nuove forme oppure frecce tra nodi esistenti; l’applicazione aggiorna lo stesso elemento sulla pagina senza creare un secondo frammento.
- Conserva identificatori di nodi e collegamenti, etichette modificate, percorsi delle frecce e geometria sulla pagina. Le etichette esistenti restano in sola lettura durante il riconoscimento: si rivede l’OCR solo per le nuove parti e si usa l’editor del diagramma per cambiare le etichette precedenti. L’ampliamento del riquadro conserva la dimensione visiva di testo e linee.
- Permette di scegliere esplicitamente la destinazione quando sono possibili più diagrammi, oppure di crearne uno separato. Conserva l’inchiostro incerto ed evita di duplicare la geometria dei tratti originali mantenuti sul foglio. Una proposta senza aggiunte valide non può essere applicata.
- Applica la sostituzione dell’oggetto e l’eventuale rimozione dei tratti in un’unica operazione Annulla/Ripeti della pagina. Il recupero persistente conserva un diagramma di base modificabile e i tratti rimossi dalle aggiunte; le estensioni successive sullo stesso livello condividono quella base. Il recupero ripristina il diagramma originale invece di eliminarlo.
- Introduce il formato portabile **10** quando servono `styleScale` o `handwritingSource.baseDiagram` e i metadati di versione 10 per i nuovi salvataggi locali. Aggiorna entrambe le app prima di trasferire questi quaderni; le esportazioni portabili con contenuti precedenti continuano a scegliere il formato minimo necessario.
- **Verifiche:** superate le suite Mac e ARM64 interessate e le prove di avvio con rendering. Sul tablet, aggiungere una forma e una freccia ha aggiornato il diagramma esistente conservandone le etichette.

Vedi [estensione dei diagrammi](docs/DIAGRAMMI.md#ampliare-un-diagramma-già-convertito) e [compatibilità dei dati](docs/DATI.md#versioni-della-libreria).

## 0.2.9 in sviluppo

- Estende la sincronizzazione automatica dei quaderni al Wi-Fi locale e al Bluetooth LE abbinato. Puoi scegliere il collegamento automatico con preferenza Wi-Fi, solo Wi-Fi oppure solo Bluetooth. L’automatismo non accende una radio Bluetooth disattivata.
- Aggiunge la sincronizzazione dopo le modifiche salvate, con una pausa dall’ultima modifica. Restano le modalità all’apertura e a intervalli; i quaderni selezionati vengono ricontrollati quando attivi l’automatismo o il dispositivo si ricollega. Le nuove installazioni restano manuali per impostazione predefinita.
- Applica i vincoli sugli SSID attendibili solo al Wi-Fi; l’eventuale requisito di ricarica del tablet vale per entrambi i collegamenti. Conserva i blocchi immediati per PIN, sospensione ed editor, la revisione del primo trasferimento, la politica sui conflitti e la revisione manuale degli esiti di applicazione incerti.
- Migliora il recupero con attese crescenti tra tentativi di collegamento, riprove limitate del trasferimento e un punto di ripresa salvato per il lavoro automatico interrotto prima dell’applicazione. Riusa le parti ricevute compatibili attraverso una nuova anteprima, senza ripetere una conferma precedente.
- Considera incerti i trasferimenti precedenti rimasti pendenti senza fase/versione salvata e le applicazioni manuali interrotte: il recupero automatico non può reinviarli senza una nuova revisione manuale. Le 11 verifiche mirate di migrazione/ripresa sono superate sia su Mac sia su ARM.
- **Verifiche:** superate le suite Mac e ARM64 interessate e le prove di avvio con rendering. Sul tablet erano disponibili le tre opzioni di collegamento e la sincronizzazione dopo le modifiche. I trasferimenti tra dispositivi richiedevano una prova separata.

Vedi [sincronizzazione automatica](docs/PROTOCOLLO_SYNC.md#sincronizzazione-automatica-nella-029) e [impostazioni](docs/IMPOSTAZIONI.md).

## 0.2.8 in sviluppo

- Aggiunta **Diagrammi** come quinta sezione della libreria, con quaderni vuoti accessibili anche prima del primo diagramma salvato. Aprire una voce mostra la pagina sorgente senza aprire automaticamente un editor.
- Riconoscimento locale di forme e frecce supportate nei diagrammi a mano libera. Una pausa nel disegno può proporre un suggerimento non modale nei quaderni Diagrammi; resta disponibile il riconoscimento esplicito con il lazo. Etichette e diagramma modificabile si rivedono prima di applicare. Tratti non riconosciuti ed etichette escluse restano inchiostro; la conversione supporta un unico annullamento della pagina e il recupero persistente dei tratti originali quando vengono sostituiti.
- Riconoscimento più tollerante per chiusure ripassate degli ovali e piccole code, interruzioni limitate dei contorni, rombi leggermente inclinati in due tratti, aste lievemente curve, punte a V o due alette, triangoli chiusi o ripassati e un gesto continuo supportato dall’asta alla punta triangolare. Forme personalizzate ambigue e collegamenti incerti restano inchiostro.
- Notifiche dedicate per dispositivo abbinato e quaderni fissati nelle automazioni di sincronizzazione, evitando che variazioni di stato estranee riattivino quei binding. Le suite interessate impostazioni/BLE/sync sono superate 3/3 sia su Mac sia su ARM.
- Riuso di diagrammi modificabili con **Inserisci dalla libreria**. Le copie sono indipendenti; modificare dalla pagina aperta nell’indice cambia il proprio oggetto originale. Il Markdown con allegati include ora PNG del diagramma e file `.drawio` modificabile; i trasferimenti dei quaderni conservano geometria e tratti recuperabili.
- Orientamento automatico opzionale sul Paper Pro con sensore supportato, filtro di stabilità e cambio differito durante le interazioni. Resta l’orientamento manuale; un orientamento di lettura fisso ha precedenza. La rotazione automatica è disattivata per impostazione iniziale.
- **Verifiche:** superate le prove di avvio, geometria e interfaccia su Mac e ARM64 dopo le correzioni al riconoscimento. Sul tablet sono state verificate la rotazione automatica e la revisione e modifica di un diagramma di prova.

Vedi [Diagrammi](docs/DIAGRAMMI.md) e [impostazioni](docs/IMPOSTAZIONI.md).

## 0.2.7 in sviluppo

- Venti nuove impostazioni per profili d’uso, ordine degli strumenti e mano preferita, fuso orario, contrasto, gesture, lettura per singolo libro, pressione, filtro del palmo, inserimento con lazo, correzioni OCR, tolleranza delle forme, profili di refresh, Wi-Fi attendibile e sync programmato, conflitti, backup, cache e destinazioni Apple preferite. Vedi [impostazioni](docs/IMPOSTAZIONI.md).
- Dialoghi Google Drive, Dropbox e OneDrive con accesso OAuth ufficiale. Esplorazione delle cartelle, download dei documenti supportati, caricamento di nuove copie e trasferimento di quaderni `.quaderno` modificabili con anteprima d’importazione. Il primo accesso Google dal Paper Pro usa il browser del Mac attraverso un inoltro SSH temporaneo; i trasferimenti successivi avvengono direttamente dal tablet via Wi-Fi.
- I token cloud restano esclusi da esportazioni e backup dei quaderni. macOS usa il Portachiavi; il Paper Pro usa file privati. Blocco e sospensione annullano autorizzazioni e trasferimenti in corso.
- L’accesso ai provider richiede le registrazioni OAuth di Quaderno. Verificata l’apertura delle finestre degli account sul tablet. Autenticazione e trasferimenti cloud richiedevano prove separate.

Superati i test automatici su Mac. Le prove di regressione dell’interfaccia coprono lo stato senza account e l’apertura delle finestre dalle impostazioni del tablet.

## Sviluppo 0.2.6

- Impostazioni per categorie con ricerca e controlli rapidi, quattro orientamenti manuali, tre dimensioni dell’interfaccia, barra di stato/orologio/indicatore della penna facoltativi, ingresso a pagina intera e preferenze delle gesture dei libri. Il ripristino della visualizzazione conserva quaderni, account, sicurezza e preferenze energetiche.
- Informazioni separate per batteria del tablet e del Marker, con letture asincrone dal firmware e gestione delle percentuali vecchie o sconosciute. Intervallo di sospensione configurabile e opzione per restare attivi durante la ricarica, conservando la politica indipendente del PIN e il comando Riposa adesso.
- Pannello Account e servizi per Obsidian, Apple/iCloud, Google Drive, Dropbox, OneDrive e Quaderno wireless, distinguendo funzioni dirette sul Paper Pro da browser e file gestiti sul Mac. Nessuna nuova autenticazione cloud OAuth o accesso diretto a iCloud dichiarati.
- [Documentazione delle impostazioni](docs/IMPOSTAZIONI.md) in inglese e italiano, percorsi aggiornati e prove di regressione per preferenze e interazioni.

Superate le suite Mac e ARM64 e le prove d’avvio dei pacchetti. Sul tablet sono stati verificati lo stato della batteria del Marker e i tocchi su tabelle e grafici in orizzontale a destra. Le prove automatiche coprono i quattro orientamenti. [Stato delle verifiche](docs/IMPOSTAZIONI.md#ambito-delle-verifiche)

## Sviluppo 0.2.5

Modifica diretta dei grafici sul foglio: un tocco del dito, un clic del mouse o un tocco breve della penna/Marker apre l’editor del grafico. I grafici indipendenti aprono le proprie proprietà; quelli collegati a una tabella aprono impostazioni e scelta delle colonne sorgente, mantenendo il collegamento. Muovere o tenere appoggiata la penna continua la scrittura. Lazo, gomma, evidenziatore e strumenti per le forme conservano il comportamento esistente; Elementi rimane disponibile per spostare e modificare gli oggetti. Aprire un elemento dalla libreria continua a mostrare soltanto il foglio.

Superate la compilazione, le suite Mac e ARM64 e le prove di avvio con rendering. Sul Paper Pro dito e tocco breve del Marker hanno aperto l’editor del grafico. [Modifica diretta dei grafici](docs/LASSO.md#modificare-un-grafico-con-un-tocco)

## Sviluppo 0.2.4

Aprire un elemento esistente da **Tabelle** o **Grafici** mostra la pagina di origine. Per modificare una tabella, tocca una cella. Per un grafico, attiva **Elementi** e toccalo. Superate le suite Mac e ARM64, gli avvii con rendering e l’apertura della pagina dalla libreria sul tablet. [Navigazione dalla libreria](docs/LASSO.md#aprire-tabelle-e-grafici-dalla-libreria)

## Sviluppo 0.2.3

Correzione della modifica diretta delle celle dopo la conversione della scrittura. Il tocco su una cella apre l’editor sulla sua riga e colonna e mostra la formula originale modificabile. Dito e mouse aprono anche le celle vuote; un tocco breve della penna apre le celle compilate, mentre nelle celle vuote la penna continua a scrivere. Muovere o tenere appoggiata la penna continua la scrittura; anche la modalità Elementi apre la cella selezionata.

Superate la compilazione, le suite Mac e ARM64 e le prove di avvio con rendering. Sul tablet sono stati verificati i tocchi con dito e Marker, la modifica delle celle e il ricalcolo delle formule. [Modifica delle celle e stato della verifica](docs/LASSO.md#modificare-una-cella-dopo-la-conversione)

## Sviluppo 0.2.2

Scrittura nelle celle delle tabelle: il lazo raggruppa i tratti per posizione, il riconoscimento locale trascrive ogni cella separatamente e un dialogo permette di correggere i valori, escludere celle e autorizzare la sostituzione di dati o formule esistenti. Applicazione atomica con rimozione facoltativa dei tratti e Annulla/Ripeti nella sessione corrente; celle non coinvolte e grafici collegati conservano il loro comportamento. Il formato dei dati resta compatibile con la versione 9. Consulta [utilizzo, limiti e stato della verifica](docs/LASSO.md#inserire-la-scrittura-nelle-celle-di-una-tabella).

## Sviluppo 0.2.1

Anteprima e formattazione Markdown, protezione delle bozze, comandi di modifica a schermo, controlli precisi del lazo misto, anteprima dei modelli e recupero del livello nascosto. Nuovi aggregati e guida alle formule, calibrazione degli assi negativi, navigazione di pagine/libreria più rapida, resoconti di importazione con ripetizione mirata, comandi wireless e Bluetooth più chiari, ricerca e filtri per Obsidian e passaggio alle app Apple. Consulta le [31 migliorie di usabilità e lo stato delle verifiche](docs/USABILITA_0_2_1.md).

## Sviluppo 0.2.0

Conversione della scrittura reversibile sul foglio; selezione mista, trasformazioni e regole del lazo; riconoscimento italiano/inglese; salvataggi transazionali per pagina; lettore asincrono da sorgenti; sync incrementale riprendibile; stati Apple persistenti; pausa dell’app coordinata; suite di regressione desktop e firmware. Consulta le [note di aggiornamento e migrazione](docs/AGGIORNAMENTO_0_2.md).

## Anteprima 0.1.0

Le funzionalità seguenti sono disponibili nei sorgenti dell’anteprima.

### Libreria e scrittura

- Libreria locale con sottocartelle, preferiti, tag, ricerca, ordinamento e cestino con ripristino.
- Panoramica con miniature, duplicazione, riordino e trasferimento di pagine tra quaderni.
- Modelli di pagina, strumenti di scrittura, livelli d’inchiostro, annulla e ripeti, blocchi di testo e tabelle.
- Modifica dei blocchi di testo con un tocco. Importazione CSV o TSV in tabelle con celle modificabili, formule di somma e media e collegamenti ai grafici.
- Forme intelligenti con gesto di attesa, anteprima e ripristino del tratto originale. Il lazo sposta, ridimensiona, ruota e copia i tratti del livello attivo, con anteprima prima della conferma.
- Conversione locale di schizzi in grafici vettoriali a linea singola o a barre, con modifica dei punti ed esportazione CSV dopo la calibrazione.
- Grafici a linee e a barre collegati a colonne di una tabella nella stessa pagina. Il grafico si aggiorna quando cambiano le celle.
- Editor di diagrammi di flusso con forme ridimensionabili, griglia, frecce ad angolo retto, modello di partenza e importazione ed esportazione `.drawio`. I diagrammi sono inclusi nei trasferimenti dei quaderni e nelle esportazioni PDF.
- Lazo per selezionare e spostare più forme, comandi di allineamento e distribuzione, zoom e spostamento della vista nell’editor di diagrammi.

### Documenti e piattaforme

- Importazione di PDF ed EPUB senza DRM a impaginazione fluida, annotazione ed esportazione PDF. Zoom, adattamento alla larghezza, segnalibri e ricerca nel testo incorporato dei PDF appena importati.
- Importazione di Markdown modificabile da file, Finder o URL HTTPS diretto ed esportazione del testo in Markdown. Creazione di nuove note in un vault Obsidian su Mac, apertura e copia dei collegamenti Obsidian Publish.
- Collegamento del Paper Pro a un vault remoto Obsidian Sync via Wi‑Fi. Apertura di note Markdown collegate, invio esplicito delle modifiche al testo e conservazione di copie separate in caso di conflitto.
- Importazione della libreria da Paper Pro e invio di singoli quaderni dal Mac via USB‑C, con anteprima delle modifiche in entrambe le direzioni e copie separate in caso di conflitto. Condivisione PDF con il menu di macOS.
- Trasferimento e sincronizzazione di quaderni selezionati via Bluetooth LE o Wi‑Fi locale tra le due app Quaderno. Abbinamento con codice, collegamento cifrato e conservazione delle copie in conflitto.
- Ripresa dalla stessa pagina su Mac e Paper Pro e stato di sincronizzazione per ogni quaderno, con data dell’ultimo trasferimento riuscito.
- Importazione tramite trascinamento dal Finder ed esportazione PDF della pagina corrente o del quaderno tramite trascinamento.
- Trascrizione offline in italiano sul Paper Pro con il motore opzionale WritePad e su macOS 27 o successivo con il componente di trascrizione. Sostituzione dell’inchiostro selezionato sulla stessa pagina o salvataggio su una nuova.
- Scelta della lingua italiana o inglese per l’interfaccia e scelta indipendente tra QWERTY italiano e inglese USA per la tastiera a schermo.
- Impostazioni Paper Pro per Wi‑Fi, diagnostica USB‑C, abbinamento Bluetooth, luce di lettura quando disponibile e refresh automatico. L’editor offre un refresh completo manuale. La tastiera a schermo si nasconde quando viene rilevata una tastiera Bluetooth o USB.
- Script per la prova temporanea via USB‑C e collegamento opzionale tramite AppLoad.

### Documentazione

- Guide in inglese e italiano per installazione, utilizzo, backup, sviluppo e preparazione del rilascio.
- Istruzioni per contributi e segnalazioni di sicurezza.
