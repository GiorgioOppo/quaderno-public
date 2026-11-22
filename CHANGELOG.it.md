# Cronologia delle modifiche

[English](CHANGELOG.md) · **Italiano**

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
- **Verifiche:** superate tutte le 20 suite macOS. Le verifiche finali di post-it, appendice PDF, sincronizzazione e riconoscimento dei diagrammi sono passate su Mac e ARM. Entrambe le build hanno superato la prova di avvio con interfaccia desktop e tablet. Installata via Wi-Fi sul Paper Pro dopo un backup verificato sul tablet. L’esportazione della libreria aveva lo stesso SHA-256 prima e dopo l’aggiornamento. Superati l’avvio nativo e-ink a 1620 × 2160 e l’apertura da AppLoad. La riproduzione locale di un disegno reale ha recuperato quattro forme e quattro frecce esplicite e ha conservato a penna una linea senza punta. La prova fisica di scrittura e trasferimento dei documenti richiede controlli separati.

Vedi [note sui libri](docs/NOTE_LIBRI.md), [flussi per sezione](docs/FUNZIONALITA.md), [sincronizzazione](docs/PROTOCOLLO_SYNC.md) e [compatibilità dei dati](docs/DATI.md#versioni-della-libreria).

## 0.2.10 in sviluppo

- Riconosce le aggiunte a mano libera nel contesto di un diagramma di flusso già convertito. L’anteprima contiene l’intero diagramma aggiornato, con nuove forme oppure frecce tra nodi esistenti; l’applicazione aggiorna lo stesso elemento sulla pagina senza creare un secondo frammento.
- Conserva identificatori di nodi e collegamenti, etichette modificate, percorsi delle frecce e geometria sulla pagina. Le etichette esistenti restano in sola lettura durante il riconoscimento: si rivede l’OCR solo per le nuove parti e si usa l’editor del diagramma per cambiare le etichette precedenti. L’ampliamento del riquadro conserva la dimensione visiva di testo e linee.
- Permette di scegliere esplicitamente la destinazione quando sono possibili più diagrammi, oppure di crearne uno separato. Conserva l’inchiostro incerto ed evita di duplicare la geometria dei tratti originali mantenuti sul foglio. Una proposta senza aggiunte valide non può essere applicata.
- Applica la sostituzione dell’oggetto e l’eventuale rimozione dei tratti in un’unica operazione Annulla/Ripeti della pagina. Il recupero persistente conserva un diagramma di base modificabile e i tratti rimossi dalle aggiunte; le estensioni successive sullo stesso livello condividono quella base. Il recupero ripristina il diagramma originale invece di eliminarlo.
- Introduce il formato portabile **10** quando servono `styleScale` o `handwritingSource.baseDiagram` e i metadati di versione 10 per i nuovi salvataggi locali. Aggiorna entrambe le app prima di trasferire questi quaderni; le esportazioni portabili con contenuti precedenti continuano a scegliere il formato minimo necessario.
- **Verifiche:** le build Mac e ARM hanno superato tutte le sette suite interessate: regressioni, lazo, riconoscimento diagrammi, riconoscimento contestuale, sincronizzazione incrementale, UI e integrazione diagrammi (86,20 s su Mac, 81,91 s su ARM; due esclusioni previste su ARM per funzioni solo macOS). Superati gli avvii rigorosi desktop/tablet e dei pacchetti estratti. Installata sul Paper Pro dopo un nuovo backup e verificato l’avvio nativo e-paper/AppLoad a 1620 × 2160. Tutti i sei quaderni hanno mantenuto hash canonici, contenuti delle cartelle e numero di byte esportati. Il 18 novembre 2026 l’utente ha confermato sul Paper Pro che una nuova forma e freccia a mano libera aggiornano il diagramma completo come unico elemento modificabile, conservando le etichette precedenti. La sessione AppLoad è stata inoltre osservata per 72,36 secondi senza errori QML. L’app Mac **0.2.0** installata è invariata; è pronto un pacchetto 0.2.10 separato. Aggiorna entrambe le app prima di trasferire quaderni in formato 10.

Vedi [estensione dei diagrammi](docs/DIAGRAMMI.md#ampliare-un-diagramma-già-convertito) e [compatibilità dei dati](docs/DATI.md#versioni-della-libreria).

## 0.2.9 in sviluppo

- Estende la sincronizzazione automatica dei quaderni al Wi-Fi locale e al Bluetooth LE abbinato. Puoi scegliere il collegamento automatico con preferenza Wi-Fi, solo Wi-Fi oppure solo Bluetooth. L’automatismo non accende una radio Bluetooth disattivata.
- Aggiunge la sincronizzazione dopo le modifiche salvate, con una pausa dall’ultima modifica. Restano le modalità all’apertura e a intervalli; i quaderni selezionati vengono ricontrollati quando attivi l’automatismo o il dispositivo si ricollega. Le nuove installazioni restano manuali per impostazione predefinita.
- Applica i vincoli sugli SSID attendibili solo al Wi-Fi; l’eventuale requisito di ricarica del tablet vale per entrambi i collegamenti. Conserva i blocchi immediati per PIN, sospensione ed editor, la revisione del primo trasferimento, la politica sui conflitti e la revisione manuale degli esiti di applicazione incerti.
- Migliora il recupero con attese crescenti tra tentativi di collegamento, riprove limitate del trasferimento e un punto di ripresa salvato per il lavoro automatico interrotto prima dell’applicazione. Riusa le parti ricevute compatibili attraverso una nuova anteprima, senza ripetere una conferma precedente.
- Considera incerti i trasferimenti precedenti rimasti pendenti senza fase/versione salvata e le applicazioni manuali interrotte: il recupero automatico non può reinviarli senza una nuova revisione manuale. Le 11 verifiche mirate di migrazione/ripresa sono superate sia su Mac sia su ARM.
- **Verifica:** sono superati build Mac e ARM, sei suite interessate per piattaforma (preferenze, sync BLE, sync incrementale, collegamento wireless, UI e integrazione delle impostazioni) e avvii rigorosi renderizzati desktop/tablet. ARM ha completato 6/6 suite in 136,73 secondi, con una sola esclusione UI prevista per macOS. Installata sul Paper Pro dopo un nuovo backup, con avvio nativo e-paper/AppLoad a 1620 × 2160 verificato. Tutti i sei quaderni hanno conservato hash canonici, contenuti delle cartelle e numero di byte esportati. Il 18 novembre 2026 l’utente ha confermato che nelle impostazioni Wi-Fi/Bluetooth del Paper Pro sono visibili la modalità dopo le modifiche e tutte e tre le scelte di collegamento. La conferma riguarda i controlli, non i trasferimenti completi; restano le prove fisiche di invio Wi-Fi/Bluetooth tra dispositivi. L’app Mac 0.2.0 installata resta invariata; un pacchetto Mac 0.2.9 separato è pronto. Per il nuovo flusso entrambe devono essere aggiornate, aperte e sbloccate; si tratta di sincronizzazione locale nell’app, non di un servizio cloud Internet o di un processo sempre attivo.

Vedi [sincronizzazione automatica](docs/PROTOCOLLO_SYNC.md#sincronizzazione-automatica-nella-029) e [impostazioni](docs/IMPOSTAZIONI.md).

## 0.2.8 in sviluppo

- Aggiunta **Diagrammi** come quinta sezione della libreria, con quaderni vuoti accessibili anche prima del primo diagramma salvato. Aprire una voce mostra la pagina sorgente senza aprire automaticamente un editor.
- Riconoscimento locale di forme e frecce supportate nei diagrammi a mano libera. Una pausa nel disegno può proporre un suggerimento non modale nei quaderni Diagrammi; resta disponibile il riconoscimento esplicito con il lazo. Etichette e diagramma modificabile si rivedono prima di applicare. Tratti non riconosciuti ed etichette escluse restano inchiostro; la conversione supporta un unico annullamento della pagina e il recupero persistente dei tratti originali quando vengono sostituiti.
- Riconoscimento più tollerante per chiusure ripassate degli ovali e piccole code, interruzioni limitate dei contorni, rombi leggermente inclinati in due tratti, aste lievemente curve, punte a V o due alette, triangoli chiusi o ripassati e un gesto continuo supportato dall’asta alla punta triangolare. Forme personalizzate ambigue e collegamenti incerti restano inchiostro.
- Notifiche dedicate per dispositivo abbinato e quaderni fissati nelle automazioni di sincronizzazione, evitando che variazioni di stato estranee riattivino quei binding. Le suite interessate impostazioni/BLE/sync sono superate 3/3 sia su Mac sia su ARM.
- Riuso di diagrammi modificabili con **Inserisci dalla libreria**. Le copie sono indipendenti; modificare dalla pagina aperta nell’indice cambia il proprio oggetto originale. Il Markdown con allegati include ora PNG del diagramma e file `.drawio` modificabile; i trasferimenti dei quaderni conservano geometria e tratti recuperabili.
- Orientamento automatico opzionale sul Paper Pro con sensore supportato, filtro di stabilità e cambio differito durante le interazioni. Resta l’orientamento manuale; un orientamento di lettura fisso ha precedenza. La rotazione automatica è disattivata per impostazione iniziale.
- **Verifiche:** la prima build 0.2.8 ha superato 15 suite Mac (129,56 secondi) e ARM (130,23 secondi), 24 verifiche geometriche, 17 UI (38,82 secondi), avvii rigorosi e controlli dei pacchetti estratti. L’installazione sul Paper Pro ha superato l’avvio nativo a 1620 × 2160 senza errori QML; cinque quaderni hanno mantenuto hash canonici, cartelle e numero di byte esportati. Il 18 novembre 2026 l’utente ha confermato la rotazione automatica da verticale a orizzontale. Il riconoscimento fisico inizialmente non ha trovato forme in un disegno reale, da cui le correzioni di tolleranza sopra indicate. Le build corrette Mac e ARM hanno superato 34 verifiche geometriche e 17 UI (2/2 suite: 37,43 secondi su Mac, 37,37 secondi su ARM), con avvii rigorosi desktop/tablet. La build corretta è installata sul Paper Pro e ha superato l’avvio rigoroso nativo/AppLoad a 1620 × 2160 senza errori QML. È stato creato un nuovo backup; tutti i sei quaderni attuali hanno mantenuto hash canonici, contenuti delle cartelle e numero di byte esportati. Il 18 novembre 2026 l’utente ha confermato che riconoscimento, applicazione del risultato e apertura dell’editor per modificarlo funzionano nel diagramma di prova sul Paper Pro; gli archivi precedenti sono conservati separatamente. L’app Mac 0.2.0 installata resta invariata. Account OAuth reali e trasferimenti richiedono ancora registrazioni del manutentore e verifiche.

Vedi [Diagrammi](docs/DIAGRAMMI.md) e [impostazioni](docs/IMPOSTAZIONI.md).

## 0.2.7 in sviluppo

- Venti nuove impostazioni per profili d’uso, ordine degli strumenti e mano preferita, fuso orario, contrasto, gesture, lettura per singolo libro, pressione, filtro del palmo, inserimento con lazo, correzioni OCR, tolleranza delle forme, profili di refresh, Wi-Fi attendibile e sync programmato, conflitti, backup, cache e destinazioni Apple preferite. Vedi [impostazioni](docs/IMPOSTAZIONI.md).
- Dialoghi Google Drive, Dropbox e OneDrive con accesso OAuth ufficiale. Esplorazione delle cartelle, download dei documenti supportati, caricamento di nuove copie e trasferimento di quaderni `.quaderno` modificabili con anteprima d’importazione. Il primo accesso Google dal Paper Pro usa il browser del Mac attraverso un inoltro SSH temporaneo; i trasferimenti successivi avvengono direttamente dal tablet via Wi-Fi.
- I token cloud restano esclusi da esportazioni e backup dei quaderni. macOS usa il Portachiavi; il Paper Pro usa file privati. Blocco e sospensione annullano autorizzazioni e trasferimenti in corso.
- Servono ancora le registrazioni OAuth di Quaderno per accedere davvero agli account. Non vengono presi in prestito né inventati identificativi sviluppatore. L’utente ha confermato che le tre finestre degli account cloud si aprono sul Paper Pro; accesso reale ai provider e trasferimenti cloud restano da verificare. App Mac installata invariata.

La prima build Mac ha superato tutte le 12 suite automatiche CTest (34,47 secondi). Le correzioni successive normalizzano i binding booleani del dialogo cloud e lo collegano all’overlay della finestra, impedendo al Loader senza dimensioni di nasconderlo. Le nuove regressioni coprono account iniziale vuoto e percorso reale Main/impostazioni/Loader. I pacchetti Mac e Paper Pro ricompilati hanno superato i test interessati e i controlli d’avvio rafforzati; l’installazione nativa corretta sul Paper Pro si avvia senza gli avvisi. Il 18 novembre 2026 l’utente ha confermato che appaiono tutte le finestre Google Drive, Dropbox e OneDrive. Questo non verifica accesso o trasferimenti, che richiedono ancora registrazioni OAuth ufficiali e prove con account reali. Archivi e resoconti precedenti restano conservati come evidenza superata. Eseguibile e informazioni di versione dell’app Mac 0.2.0 installata restano invariati.

## Sviluppo 0.2.6

- Impostazioni per categorie con ricerca e controlli rapidi, quattro orientamenti manuali, tre dimensioni dell’interfaccia, barra di stato/orologio/indicatore della penna facoltativi, ingresso a pagina intera e preferenze delle gesture dei libri. Il ripristino della visualizzazione conserva quaderni, account, sicurezza e preferenze energetiche.
- Informazioni separate per batteria del tablet e del Marker, con letture asincrone dal firmware e gestione delle percentuali vecchie o sconosciute. Intervallo di sospensione configurabile e opzione per restare attivi durante la ricarica, conservando la politica indipendente del PIN e il comando Riposa adesso.
- Pannello Account e servizi per Obsidian, Apple/iCloud, Google Drive, Dropbox, OneDrive e Quaderno wireless, distinguendo funzioni dirette sul Paper Pro da browser e file gestiti sul Mac. Nessuna nuova autenticazione cloud OAuth o accesso diretto a iCloud dichiarati.
- [Documentazione delle impostazioni](docs/IMPOSTAZIONI.md) in inglese e italiano, percorsi aggiornati e prove di regressione per preferenze e interazioni.

Tutte le otto suite automatiche e gli avvii dei pacchetti sono superati su Mac e ARM; le prove UI/avvio interessate sono state ripetute con successo dopo la correzione finale. Installata e aperta tramite AppLoad sul Paper Pro, con rendering completo a 1620 × 2160, controllo rigoroso degli errori QML superato e conservazione dei cinque quaderni, delle cartelle e delle esportazioni. La telemetria nativa del Marker è stata verificata e ricontrollata dopo il riavvio. L’utente ha confermato **100% / Carica completa** nell’indicatore della penna e i tocchi corretti con dito/Marker breve su tabelle e grafici in **orizzontale a destra (90°)**. Le prove automatiche coprono i quattro angoli; la conferma fisica della rotazione riguarda i 90° a destra. L’orologio segue il fuso orario del sistema del dispositivo; Quaderno non offre un selettore del fuso. L’app Mac 0.2.0 installata rimane invariata. Consulta lo [stato delle verifiche](docs/IMPOSTAZIONI.md#ambito-delle-verifiche).

## Sviluppo 0.2.5

Modifica diretta dei grafici sul foglio: un tocco del dito, un clic del mouse o un tocco breve della penna/Marker apre l’editor del grafico. I grafici indipendenti aprono le proprie proprietà; quelli collegati a una tabella aprono impostazioni e scelta delle colonne sorgente, mantenendo il collegamento. Muovere o tenere appoggiata la penna continua la scrittura. Lazo, gomma, evidenziatore e strumenti per le forme conservano il comportamento esistente; Elementi rimane disponibile per spostare e modificare gli oggetti. Aprire un elemento dalla libreria continua a mostrare soltanto il foglio.

Superate compilazioni e tutte le sei suite su Mac (85 casi funzionali e 12 verifiche di inizializzazione/chiusura) e ARM (82 e 12, con tre esclusioni previste per funzioni macOS), insieme agli avvii renderizzati desktop/tablet su entrambe le piattaforme. Passano tutte le otto nuove prove dei grafici e le regressioni della navigazione 0.2.4. Installata sul Paper Pro tramite USB il 18 novembre 2026, con verifica dell’avvio nativo, del binario in esecuzione tramite AppLoad, dei backup e dell’integrità dei quaderni. Lo stesso giorno l’utente ha confermato sul Paper Pro l’apertura dell’editor del grafico esistente provato con dito e tocco breve della penna, senza attivare Elementi. L’app Mac 0.2.0 è rimasta invariata. Consulta la [modifica diretta dei grafici](docs/LASSO.md#modificare-un-grafico-con-un-tocco).

## Sviluppo 0.2.4

Aprire un elemento esistente da **Tabelle** o **Grafici** mostra la pagina di origine senza aprire automaticamente l’editor. Per modificare una tabella, tocca una cella; per un grafico, attiva **Elementi** e toccalo. Superate compilazioni e tutte le sei suite su Mac (77 casi funzionali e 12 verifiche di inizializzazione/chiusura) e ARM (74 e 12, con tre esclusioni previste per funzioni macOS), insieme agli avvii renderizzati desktop/tablet su entrambe le piattaforme. Tre nuove prove UI riproducono il difetto originale e passano con la correzione. Installata sul Paper Pro tramite USB il 18 novembre 2026, con verifica dell’avvio nativo, del binario installato, dei backup e dell’integrità dei quaderni. Lo stesso giorno l’utente ha confermato sul Paper Pro che aprire elementi esistenti da Tabelle e Grafici mostra il foglio senza aprire automaticamente l’editor. L’app Mac 0.2.0 è rimasta invariata. Consulta la [navigazione dalla libreria](docs/LASSO.md#aprire-tabelle-e-grafici-dalla-libreria).

## Sviluppo 0.2.3

Correzione della modifica diretta delle celle dopo la conversione della scrittura. Il tocco su una cella apre l’editor sulla sua riga e colonna e mostra la formula originale modificabile. Dito e mouse aprono anche le celle vuote; un tocco breve della penna apre le celle compilate, mentre nelle celle vuote la penna continua a scrivere. Muovere o tenere appoggiata la penna continua la scrittura; anche la modalità Elementi apre la cella selezionata.

Compilazioni Mac e SDK ARM riuscite, con sei suite superate per piattaforma: 74 casi funzionali su Mac e 71 su ARM, oltre a 12 verifiche di inizializzazione/chiusura per piattaforma e tre esclusioni previste su ARM per funzioni macOS. Superati gli avvii renderizzati su Mac nei layout desktop e tablet ed entrambi gli avvii con l’SDK ARM. Installata sul Paper Pro tramite USB il 18 novembre 2026, con verifica dell’avvio nativo, del binario in esecuzione tramite AppLoad, dei backup e dell’integrità dei quaderni. Lo stesso giorno l’utente ha confermato tocchi con dito e penna, modifica e ricalcolo delle formule nel caso di prova fisico **QA Celle 0.2.2**. L’app Mac 0.2.0 è rimasta invariata. Consulta [modifica delle celle e stato della verifica](docs/LASSO.md#modificare-una-cella-dopo-la-conversione).

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
