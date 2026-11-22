# Lazo, scrittura a mano e selezioni modificabili

Il lazo agisce sui tratti del livello attivo visibile e sugli elementi della pagina: testo, immagini, tabelle, grafici e diagrammi. I tratti degli altri livelli restano esclusi.

## Selezionare e correggere la selezione

1. Apri un documento e scegli **Lazo**.
2. Scegli **Elementi contenuti** per includere soltanto tratti ed elementi interi, oppure **Elementi toccati** per includere ciò che il contorno attraversa. Puoi selezionare anche punti e accenti molto piccoli.
3. Disegna un contorno chiuso. La selezione rimane sul foglio, con un riquadro e le maniglie di trasformazione.
4. Usa **Aggiungi** o **Sottrai** per disegnare un altro contorno e correggere la selezione.

La modalità contenuti controlla anche i segmenti che attraversano una rientranza del contorno. I contorni molto densi vengono semplificati con una piccola tolleranza geometrica. Se sono eccessivamente complessi, Quaderno chiede di ridisegnarli.

## Spostare, ridimensionare e ruotare

Trascina il riquadro per spostare la selezione, la maniglia inferiore per ridimensionarla e quella superiore per ruotarla a intervalli di 15 gradi. **Applica** salva insieme tratti ed elementi. **Annulla** abbandona l’anteprima. Se una trasformazione supera i bordi della pagina, il documento resta invariato.

Il testo, le tabelle e le immagini conservano i dati modificabili. Le celle mantengono le formule e i grafici collegati mantengono il riferimento alla tabella. I rettangoli e le ellissi rettificati che conservano i tratti originali accettano rotazioni di 90 gradi; le trasformazioni non supportate vengono rifiutate al momento di applicarle.

**Azioni → Elimina selezione** rimuove insieme i contenuti selezionati. Annulla e Ripeti agiscono sull’intera operazione. Una selezione diventata obsoleta viene rifiutata, evitando di modificare altri elementi.

## Convertire la scrittura nella stessa posizione

La lingua di riconoscimento è salvata nel documento. Scegli **Italiano** o **English** nei controlli del lazo o nel menu Azioni. Il riconoscimento usa i motori e i dizionari installati localmente: il componente opzionale WritePad sul Paper Pro, PencilKit e Vision su macOS 27 o successivo.

Sul Paper Pro il riconoscimento parte dopo una selezione contenente tratti di penna, matita o pennarello. **Trascrivi** lo avvia manualmente su entrambe le piattaforme. Evidenziature, forme geometriche e altri elementi non vengono convertiti.

Il risultato compare sulla pagina nella posizione della scrittura, mantenendo gli a capo. Puoi modificarlo con una tastiera oppure usare **Correggi** per aprire l’editor, con la tastiera a schermo quando non è collegata una tastiera hardware. Le alternative vengono mostrate soltanto quando sono fornite dal motore. **Inserisci qui** sostituisce i tratti riconosciuti con testo modificabile. L’editor mantiene anche **Salva in una nuova pagina**.

Una trasformazione interrompe il riconoscimento in corso, evitando che un risultato tardivo cancelli l’anteprima dello spostamento. Cambio pagina, blocco e standby chiudono la selezione.

## Inserire la scrittura nelle celle di una tabella

La versione 0.2.2 in sviluppo permette di aggiornare una **tabella modificabile di Quaderno già presente sulla pagina**, anche se creata importando un CSV. Scrivi direttamente nelle sue celle con penna, matita o pennarello sul livello attivo visibile. Le griglie dei modelli di pagina, le tabelle disegnate a mano e quelle contenute in un PDF o in un’immagine non sono tabelle modificabili per questa funzione.

1. Scrivi uno o più valori all’interno delle rispettive celle, evitando di attraversarne i bordi.
2. Circonda i tratti con il lazo e scegli **Inserisci nelle celle**. Prima applica o annulla eventuali trasformazioni della selezione.
3. Controlla la tabella di destinazione. Quaderno propone quella con più tratti selezionati assegnabili alle celle; puoi sceglierne un’altra nel dialogo. Il riconoscimento procede separatamente per ciascuna cella, seguendo la lingua italiana o inglese del documento e usando il motore locale disponibile.
4. Rivedi gli indirizzi delle celle, per esempio `B2`, il valore precedente e il testo riconosciuto. Correggi il testo e scegli quali celle includere. In caso di riconoscimento non riuscito puoi inserire manualmente il valore; ogni cella accetta al massimo 500 caratteri. La tastiera a schermo segue il rilevamento della tastiera fisica.
5. Per modificare celle già occupate, abilita esplicitamente la sostituzione dei valori esistenti, comprese le formule. Controlla l’opzione di rimozione dei tratti convertiti, attiva inizialmente, quindi applica le modifiche. Annullare il dialogo lascia invariati tabella e inchiostro.

L’associazione usa la posizione dei tratti rispetto alla griglia, anche per una tabella ruotata. Un tratto che attraversa un bordo o resta fuori dalla tabella non viene assegnato a una cella per supposizione: il dialogo segnala quanti tratti restano esclusi. Questi tratti e quelli delle celle non incluse restano sul foglio. Gli altri livelli, le evidenziature, le forme e gli oggetti selezionati non vengono convertiti.

L’applicazione aggiorna soltanto le celle incluse e, se richiesto, rimuove i relativi tratti. Le altre celle conservano testo e formule, la geometria della tabella rimane invariata e i grafici collegati alla stessa tabella si aggiornano. Se la selezione o la tabella è cambiata durante la revisione, l’operazione viene rifiutata senza applicare modifiche parziali: rinnova la selezione.

**Annulla e Ripeti trattano insieme celle e tratti nella sessione corrente.** La cronologia si azzera al cambio pagina o livello e alla chiusura dell’app. Per conservare la scrittura anche in seguito, disattiva la rimozione dei tratti prima di applicare: questa conversione nelle celle non salva un campo di recupero permanente e non offre **Ripristina scrittura originale**. I dati delle celle rimangono compatibili con il formato 9 e modificabili dopo il trasferimento del quaderno.

La versione 0.2.2 è stata compilata su Mac e con l’SDK ARM del Paper Pro. Il 17 novembre 2026 sono passate le sei suite su entrambe le piattaforme (70 casi funzionali su Mac e 67 su ARM, con tre esclusioni previste per funzioni macOS). Le quattro nuove prove del modello e le tre dell’interfaccia coprono associazione alle celle, rotazione, livelli, selezioni obsolete, protezione delle formule, salvataggio e trasferimento, grafici collegati, annullamento, OCR sequenziale, cancellazione e tastiera. L’OCR dei test usa risposte controllate per verificare il flusso, non la qualità del riconoscimento di una scrittura reale. Sono passati anche gli avvii renderizzati e la verifica visiva dell’anteprima nei layout Mac e tablet; dopo le ultime correzioni sono state rieseguite le suite UI e le prove di avvio.

Il 17 novembre 2026 la versione 0.2.2 è stata installata sul Paper Pro con firmware 3.29.1.0 tramite USB e registrata in AppLoad. Sono stati creati backup locale e sul tablet; il binario corrisponde al pacchetto e l’avvio nativo ha raggiunto `QUADERNO_READY` a 1620 × 2160. L’aggiunta del quaderno separato **QA Celle 0.2.2** ha conservato invariati tutti e cinque i quaderni preesistenti. L’app Mac 0.2.0 è rimasta invariata su richiesta dell’utente.

Il 18 novembre 2026 l’utente ha confermato sul Paper Pro il riconoscimento di `12` in A2 e `7` in A3, l’inserimento nelle celle e i risultati B2 = 24, B3 = 14 e C2 = 19. Il riscontro riguarda questa prova concreta; Annulla non è stato confermato nella prova fisica. È emerso un problema successivo: toccare direttamente le celle sul foglio nella modalità di scrittura non apriva l’editor. Nella 0.2.2 si può aprire la tabella tramite **Elementi**, poi scegliere la cella nell’editor.

I log locali sono `build-dev/test-table-ink-final.log`, `build-dev/test-table-ink-ui-final.log`, `build-arm/test-0.2.2-final.log`, `build-arm/test-0.2.2-ui-final.log`, `build-release/paper-pro-install-0.2.2.log` e `build-release/paper-pro-native-startup-0.2.2.log`.

## Modificare una cella dopo la conversione

La correzione in sviluppo della **0.2.3** apre l’editor sulla cella toccata direttamente sul foglio, senza dover prima attivare Elementi. Con uno strumento di scrittura selezionato e il lazo chiuso:

- **Dito:** tocca una cella, vuota o compilata, per modificarla.
- **Mouse su Mac:** fai clic su una cella, vuota o compilata, per modificarla.
- **Penna/Marker:** un tocco breve su una cella compilata apre l’editor. Nelle celle vuote la penna continua a scrivere, conservando anche i punti. Muovere o tenere appoggiata la penna in una cella compilata permette di continuare a scrivere.
- **Modalità Elementi:** tocca una cella per aprire l’editor sulla sua riga e colonna. Trascinare la tabella continua a spostarla sul foglio.

Nell’editor, la griglia seleziona la cella e il campo **Testo della cella** ne modifica il contenuto. Se la cella contiene una formula, il campo mostra la formula originale, per esempio `=SOMMA(A2:A3)`, mentre la griglia mostra il risultato. Usa la tastiera fisica o quella a schermo, poi premi **Salva**: il valore o la formula restano modificabili nella tabella.

Il 18 novembre 2026 la 0.2.3 è stata compilata su Mac e con l’SDK ARM del Paper Pro. Le sei suite sono passate su entrambe le piattaforme: 74 casi funzionali e 12 verifiche di inizializzazione/chiusura su Mac; 71 casi funzionali e 12 verifiche di inizializzazione/chiusura su ARM, con tre esclusioni previste per funzioni macOS. Su Mac, le cinque suite non UI sono passate nell’esecuzione completa e la suite UI è passata nella riesecuzione finale; su ARM tutte e sei sono passate nell’esecuzione finale. Sono passati anche gli avvii renderizzati su Mac nei layout desktop e tablet ed entrambi gli avvii con l’SDK ARM.

Il 18 novembre 2026 la 0.2.3 è stata installata sul Paper Pro tramite USB, dopo nuovi backup locali e sul tablet. La prova di avvio nativo ha raggiunto `QUADERNO_READY` a 1620 × 2160; sono stati verificati anche il ripristino dell’interfaccia originale e dell’impostazione di sospensione automatica al termine della prova. Quaderno è poi stato avviato tramite AppLoad, verificando che il checksum del binario in esecuzione corrispondesse a quello del nuovo pacchetto. Il confronto prima e dopo l’aggiornamento ha confermato invariati gli hash dei contenuti normalizzati dei cinque quaderni presenti, incluso **QA Celle 0.2.2**, e il contenuto delle cartelle. Il binario e il file delle proprietà dell’app Mac 0.2.0 sono rimasti invariati.

Il 18 novembre 2026 l’utente ha confermato sulla 0.2.3 che dito e penna aprono l’editor e che modifica e formule sono corrette. La prova proposta in **QA Celle 0.2.2** prevedeva di cambiare A2 in `15`, mantenendo A3 = 7, e verificare B2 = 30 e C2 = 22. Il riscontro riguarda questo scenario sul Paper Pro collegato; non estende il collaudo a tutte le modalità di input o ad altri dispositivi. Annulla resta non confermato nella prova fisica.

## Aprire tabelle e grafici dalla libreria

La correzione in sviluppo nella **0.2.4** apre la pagina di origine quando scegli un elemento esistente nelle sezioni **Tabelle** o **Grafici**, senza aprire automaticamente il dialogo di modifica. Puoi così vedere e annotare il foglio prima di scegliere cosa modificare.

Per modificare una tabella, tocca una cella con i gesti descritti sopra; la modifica diretta introdotta nella 0.2.3 rimane disponibile. Per modificare un grafico nella 0.2.4, attiva **Elementi** e tocca il grafico; la 0.2.5 in sviluppo aggiunge il [tocco diretto sul grafico](#modificare-un-grafico-con-un-tocco). I comandi per creare nuove tabelle o nuovi grafici continuano ad aprire i rispettivi editor.

Il 18 novembre 2026 la 0.2.4 ha superato compilazione e sei suite complete su entrambe le piattaforme: 77 casi funzionali e 12 verifiche di inizializzazione/chiusura su Mac, in 21,55 secondi; 74 casi funzionali e 12 verifiche di inizializzazione/chiusura con l’SDK ARM, in 20,18 secondi, con tre esclusioni previste per funzioni macOS. Le tre nuove prove UI coprono tabella, grafico indipendente e grafico collegato a una tabella: clic sulla riga reale della libreria, successiva modifica esplicita, apertura tramite funzione e dati invariati. Sul comportamento precedente tutte e tre fallivano precisamente perché l’editor si apriva automaticamente; con la correzione passano. Sono passati anche gli avvii renderizzati nei layout desktop e tablet su entrambe le piattaforme.

Il 18 novembre 2026 la 0.2.4 è stata installata sul Paper Pro tramite USB, dopo nuovi backup locali e sul tablet. Il checksum del binario installato corrisponde al pacchetto. Gli hash dei contenuti normalizzati dei cinque quaderni presenti e il contenuto delle cartelle sono rimasti invariati; il confronto delle esportazioni ha verificato anche la stessa dimensione in byte. La prova di avvio nativo ha raggiunto `QUADERNO_READY` a 1620 × 2160, con successivo ripristino dell’interfaccia originale e dell’impostazione di sospensione automatica. Il binario e il file delle proprietà dell’app Mac 0.2.0 sono rimasti invariati.

Il 18 novembre 2026 l’utente ha confermato sulla 0.2.4 che aprire un elemento esistente da **Tabelle** e poi da **Grafici** mostra il foglio senza aprire automaticamente l’editor. Il riscontro fisico riguarda questa navigazione nelle due sezioni sul Paper Pro collegato.

## Modificare un grafico con un tocco

La **0.2.5** in sviluppo permette di aprire l’editor toccando direttamente il grafico sul foglio, senza attivare prima Elementi. Con penna, matita o pennarello selezionati e il lazo chiuso, il comportamento previsto è:

- **Dito o mouse:** tocca o fai clic sul grafico per aprire l’editor.
- **Penna/Marker:** un tocco breve apre l’editor; muovere o tenere appoggiata la penna continua la scrittura sul foglio.
- **Grafico indipendente:** si aprono le proprietà del grafico, inclusi dati e impostazioni.
- **Grafico collegato a una tabella:** si aprono le impostazioni del grafico e la scelta delle colonne X e Y della tabella sorgente. Salvare mantiene il collegamento alla tabella.

Lazo, gomma, evidenziatore e strumenti per le forme non attivano la modifica diretta del grafico. **Elementi** resta disponibile per modificare o spostare gli oggetti. La navigazione dalla libreria conserva il comportamento della 0.2.4: aprire una voce in Grafici mostra il foglio; l’editor si apre con un successivo gesto esplicito sul grafico.

Il 18 novembre 2026 la 0.2.5 ha superato compilazione e tutte le sei suite su entrambe le piattaforme: 85 casi funzionali e 12 verifiche di inizializzazione/chiusura su Mac, in 24,83 secondi; 82 casi funzionali e 12 verifiche di inizializzazione/chiusura con l’SDK ARM, in 25,02 secondi, con tre esclusioni previste per funzioni macOS. Il riepilogo della suite UI riporta 39 verifiche superate su Mac e 38 superate con una esclusa su ARM. Sono passate tutte le otto nuove prove dei grafici: sei combinazioni di input e tipo di grafico, strumenti di annotazione e riconoscimento dei tocchi con rotazione e sovrapposizioni. Restano superate le regressioni della navigazione introdotte nella 0.2.4. Sono passati anche gli avvii renderizzati nei layout desktop e tablet su entrambe le piattaforme.

Il 18 novembre 2026 la 0.2.5 è stata installata sul Paper Pro tramite USB, dopo nuovi backup locali e sul tablet. Il checksum del binario installato corrisponde al pacchetto; è stato verificato anche che AppLoad esegua il nuovo binario. Gli hash dei contenuti normalizzati dei cinque quaderni presenti e il contenuto delle cartelle sono rimasti invariati; il confronto delle esportazioni ha verificato anche la stessa dimensione in byte. La prova di avvio nativo ha raggiunto `QUADERNO_READY` a 1620 × 2160, con successivo ripristino dell’interfaccia originale e dell’impostazione di sospensione automatica. Il binario e il file delle proprietà dell’app Mac 0.2.0 sono rimasti invariati.

Il 18 novembre 2026 l’utente ha confermato sulla 0.2.5 che un tocco del dito e un tocco breve della penna aprono l’editor del grafico esistente, con **Penna** selezionata, lazo chiuso e modalità Elementi disattivata. Questo riscontro fisico riguarda l’apertura del grafico provato sul Paper Pro collegato.

## Ripristinare la scrittura originale

Il testo convertito conserva i tratti originali e il riferimento al loro livello. Il recupero rimane disponibile dopo il cambio pagina, la chiusura dell’app e il trasferimento verso un’altra installazione aggiornata di Quaderno.

Tocca il testo convertito e scegli **Ripristina scrittura originale**, oppure seleziona quel singolo elemento con il lazo e usa la stessa voce in **Azioni**. Prima seleziona e mostra il livello originale. Se quel livello è stato eliminato, il recupero usa il livello attivo visibile. Il ripristino sostituisce il testo con i tratti ed è annullabile. Se sposti o ruoti il testo con il lazo, anche i tratti conservati seguono la nuova posizione.

Le correzioni al testo mantengono la scrittura originale. Eliminare l’elemento di testo elimina anche i tratti conservati al suo interno, recuperabili tramite Annulla nella sessione corrente. Questo dato permette di recuperare la conversione, ma non costituisce una cronologia illimitata delle versioni.

## Formato e verifiche

Il formato esteso salva `handwritingSource` nel testo convertito, gli eventuali `rotation` e `textScale` negli elementi e `handwritingLanguage` nei documenti. Per mantenere i dati di recupero modificabili, aggiorna Quaderno su entrambi i dispositivi.

I test coprono punti piccoli e contorni concavi, aggiunta e sottrazione, selezioni obsolete, recupero dopo riapertura e modifica del testo, trasferimento dei tratti originali e della lingua, trasformazioni miste e annullamento, recupero dopo rotazione e interazioni QML con selezione, schermo intero, standby e gesture. Consulta [sviluppo](SVILUPPO.md) per compilazione e test. Il comportamento fisico del display richiede una verifica sul tablet.
