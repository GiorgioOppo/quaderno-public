# Collaudo funzionale di Quaderno

[English quick start](en/FUNCTIONAL_QA.md) · [Mappa delle funzioni](FUNZIONI_E_FLUSSI.md) · [Controllo remoto](COLLAUDO_CONTROLLO_REMOTO.md)

Esegui il collaudo sulla versione candidata alla distribuzione. Registra commit, versioni di Quaderno, modelli, firmware Paper Pro, versione macOS e tipo di collegamento. Per le prove fra dispositivi usa due librerie di prova e app che dichiarano il supporto allo stesso formato dei quaderni. Conserva un backup verificato prima di qualsiasi aggiornamento. [Sviluppo](SVILUPPO.md) · [Installazione](INSTALLAZIONE.md) · [Dati e backup](DATI.md)

## Esecuzione automatica

Configura un build con `BUILD_TESTING=ON` e compila l’app e i test. Il comando controlla la registrazione delle suite attese, esegue tutte le suite CTest in un profilo temporaneo e avvia l’app con i layout desktop e tablet. Scrive `qa-results.xml` nella directory del build. Un test mancante, un errore CTest o un avvio senza fotogramma pronto restituisce un codice diverso da zero.

```sh
cmake --build build --parallel 2
python3 scripts/run-qa.py build --parallel 2
```

Usa `--ctest` se CTest si trova fuori dal `PATH` e `--binary` per collaudare l’eseguibile di un pacchetto estratto. Per un build SDK esegui il gate nell’ambiente del runtime Qt di destinazione. Conserva anche i log di avvio. Il rapporto JUnit conta le suite come unità CTest. Per i casi saltati o le differenze fra piattaforme consulta il log QtTest. [Guida alla compilazione](SVILUPPO.md) · [Codice del gate](../scripts/run-qa.py)

Se hai compilato il motore di scrittura, prova anche l’eseguibile `quaderno-handwriting` nello stesso ambiente del pacchetto. Il controllo usa sette campioni sintetici, tra cui testo su più righe, zero confondibile con la lettera O, numero con segno e virgola decimale. Per WritePad aggiunge l’identificatore `IDX01`. [Campioni sintetici](../tests/fixtures/handwriting.json)

```sh
python3 scripts/check-handwriting.py /percorso/del/pacchetto/quaderno-handwriting
```

## Dati di prova

Prepara due PDF sintetici con testo selezionabile, titolo distinto e numero stampato su ogni pagina. Usa un PDF di due pagine per le annotazioni, uno di 161 pagine per l’accesso casuale e un EPUB senza DRM con titoli, paragrafi e immagini. Aggiungi un Markdown UTF-8 con due paragrafi, un compito e un’immagine propria o generata. Per il flusso misto prepara un foglio con testo, tabella 2×3 con due celle vuote e diagramma di tre forme. Prepara una copia PDF con lo stesso contenuto per confrontare il percorso su sfondo raster con quello sui tratti.

Usa questa tabella trascrivibile in CSV e TSV. La prima riga fornisce le etichette X/Y e i valori `-3` e `7` coprono le coordinate con segno. Nell’editor imposta `C2` a `=SUM(B2:B3)` e verifica `4`. Imposta `C3` a `=AVERAGE(B2:B3)` e verifica `2`. Cambia `B2` a `-5` e verifica la nuova somma `2`, media `1` e posizione del punto nel grafico collegato.

```csv
X,Y,Calcolo
1,-3,
2,7,
```

Separa i campioni di trascrizione in italiano, inglese e numeri con segno. Per i grafici disegna una linea a cinque punti e tre barre con assi chiari. Per i diagrammi usa una forma iniziale, un rombo, due esiti con connettori e un’aggiunta da riconoscere dopo il primo salvataggio. Usa etichette sintetiche come `IN`, `OK`, `NO`. Salva i file con nomi che iniziano con `QA` e usa credenziali e account dedicati solo dove il servizio li richiede.

## Libreria, pagine e scrittura

I nomi di suite indicano la copertura automatica pertinente al flusso. Esegui ogni percorso in app con la versione candidata per registrare l’esito completo, comprese le combinazioni di tasti, il Marker, i servizi e il comportamento attraverso i dispositivi.

| Funzione | Flusso e risultato atteso | Verifica |
| --- | --- | --- |
| Navigazione e organizzazione | Crea o importa una voce per sezione. Rinomina un quaderno, spostalo in una cartella, aggiungi tag e preferito. Cerca e filtra, confronta ordinamenti, usa il cestino e ripristina. La voce riappare nella sezione e nella cartella attese. | `workspaceintegration`, `regression`. Verifica titoli lunghi e finestre strette sul Mac e sul Paper Pro. |
| Panoramica e ricerca pagina | Apri le miniature, salta alla pagina indicata, cerca testo/annotazioni, filtra per segnalibri e aggiungi tag. Duplica, riordina, sposta su un altro quaderno e infine elimina una pagina di prova. Verifica ordine, contenuto, ricerca e posizione di lettura dopo la riapertura. | `reader`, `regression`, `booknotes`, `workspaceintegration`. |
| Quaderno, modelli e livelli | Scegli ogni modello, crea e rinomina livelli, cambia livello attivo, nascondi e ripristina la visibilità. Salva un tratto di colore distinto per livello e riapri. Verifica che il PDF esportato mostri i soli livelli visibili. | `ui`, `regression`. Controlla contrasto e sfondi sul pannello E Ink. |
| Penna, gomma e forme intelligenti | Alterna penna, matita, pennarello, evidenziatore e gomma. Varia colore, spessore, pressione e mano. Tieni il Marker alla fine di linea, rettangolo ed ellisse. Controlla l’anteprima, annulla, ripeti e ripristina il tratto originale. | `ui`, `lasso`, `settingsintegration`. Verifica pressione, palmo e tratti brevi con il Marker fisico. |
| Lazo e oggetti | Seleziona per inclusione e per contatto. Aggiungi e sottrai elementi. Sposta, ridimensiona, ruota, copia e incolla. Verifica posizione e selezione dopo annullamento, ripetizione e riapertura. | `lasso`, `ui`. |
| Testo, immagini e Markdown | Crea un testo e inserisci PNG e JPEG. Apri modifiche dal foglio. Importa Markdown locale, modifica il sorgente e verifica il testo dopo riapertura. Esporta con allegati e controlla risoluzione dei collegamenti. | `regression`, `workspaceintegration`, `reader`. Prova su Mac tastiera e trascinamento dal Finder. |
| Scrittura a mano | Disegna campioni in italiano, inglese e cifre. Seleziona col lazo e trascrivi. Correggi la proposta prima di salvarla nella stessa pagina e ripeti su nuova pagina. Verifica posizione, ricerca e recupero dell’inchiostro. Registra testo atteso e testo proposto prima delle correzioni. | `lasso`, `ui` e verifica dei motori con dati sintetici. Per la qualità sul dispositivo usa scrittura reale del Marker. |
| Collegamento e copia | Collega una pagina con tabella, grafico e diagramma. Apri e modifica l’originale, torna alla pagina di partenza e riapri. Inserisci anche una copia dalla libreria e verifica che le modifiche alla copia abbiano effetto solo sulla copia. | `booknotes`, `workspaceintegration`. |

## Libri e annotazioni

| Funzione | Flusso e risultato atteso | Verifica |
| --- | --- | --- |
| PDF, EPUB e importazione in gruppo | Importa il PDF lungo e l’EPUB preparato. Apri pagine iniziale, centrale e oltre la 150, verifica il contenuto e riapri. Prova importazione in gruppo con un file illeggibile e ripeti soltanto il fallimento. Interrompi un’importazione e verifica la posizione della pagina aperta. | `reader`, `regression`. Osserva tempi e fluidità sul Paper Pro. |
| Lettura, zoom e gesture | Cambia pagina con le frecce e i gesti, prova i limiti prima/ultima pagina, zoom e pan per libro e carta, doppio tocco e adattamento alla larghezza. Apri il menu a scomparsa dal bordo **Strumenti** in verticale e orizzontale, seleziona un’azione e verifica la chiusura. Ripeti con gesture disattivate usando il pulsante. | `ui`, `settingsintegration`. Ripeti su E Ink con dito, Marker e palmo appoggiato. |
| Preferenze di lettura | Cambia zoom, margini, orientamento e scorrimento di un libro, riaprilo e controlla il ripristino. Applica le impostazioni generali e verifica il comportamento in un secondo libro. Controlla le preferenze locali dopo uno scambio con l’altro dispositivo. | `settingsintegration`, `regression`, `tabletpreferences`. |
| Evidenziazione e sottolineatura | Annota con strumenti distinti su entrambe le pagine. Verifica livelli, filtro **Pagine annotate**, zoom, rotazione e revisioni dopo riapertura. | `booknotes`, `booknotesintegration`, `ui`. |
| Post-it con testo e inchiostro | Posiziona una nota per pagina. Scrivi con tastiera e penna sulla stessa nota, riapri il marcatore, modifica e sposta. Cerca il testo, prova annullamento e controlla l’ancoraggio con zoom e rotazione. | `booknotes`, `booknotesintegration`, `workspaceintegration`. Ripeti il tratto con Marker fisico. |
| Link da un libro | Ancora collegamenti a grafico, tabella e diagramma. Aprili e usa **Torna al libro**. Trasferisci anche il quaderno di destinazione se separato. | `booknotes`, `booknotesintegration`, `workspaceintegration`. |
| PDF annotato e scambio nativo | Esporta pagina singola e intero libro, controlla richiami, testo a capo e disegni nelle appendici PDF. Importa lo stesso quaderno nativo sul secondo dispositivo, riapri la nota e modificala. | `booknotes`, `reader`, `wirelesssync`. Ripeti invio e ritorno su due app reali. |

## Tabelle, grafici e diagrammi

| Funzione | Flusso e risultato atteso | Verifica |
| --- | --- | --- |
| CSV/TSV e formule | Importa il campione, modifica celle con dito, penna e tastiera, prova la navigazione, **Annulla** e la riduzione della griglia. Calcola somma, media, min, max e conteggio. Prova un riferimento ciclico, poi correggilo. Riapri il quaderno e verifica formule, valori ed esportazione CSV. | `regression`, `ui`, `workspaceintegration`. Verifica CSV tra Mac e Paper Pro. |
| Scrittura nelle celle | Scrivi `12` e `-5` in celle vuote e altri tratti sopra una formula o fuori griglia. Rivedi destinazioni, correggi, applica e annulla. La formula si conserva senza il consenso esplicito alla sostituzione, i tratti non assegnati restano sul foglio. | `lasso`, `ui`. Prova risultati OCR di scrittura reale sul Paper Pro. |
| Tabella disegnata e pagina mista | Contorna con il Marker ogni area del foglio e ripeti su PDF. Controlla la separazione delle colonne, le celle vuote e le etichette delle forme. Prova una griglia nativa 2×3 con vuoti e un tratto a cavallo di un divisorio. Ripeti senza i divisori per verificare il riconoscimento per righe. Salva un solo quaderno e verifica modifica e navigazione di tutte le sezioni. | `workspaceintegration`. Ripeti l’OCR con tratti reali e con immagine sul Paper Pro. |
| Grafici da punti | Crea linea e barre, modifica assi, scala con minimo negativo, titolo e punti. Salva, tocca il grafico, cambia un valore e riapri. Esporta CSV e confronta i valori con segno. | `regression`, `ui`, `workspaceintegration`. |
| Grafico da tabella | Scegli colonne con intestazioni e formula. Nella stessa pagina cambia un input, controlla che formula e grafico si aggiornino. Da un altro foglio crea una copia dei valori e verifica che le modifiche successive al sorgente lascino quella copia con i valori iniziali. | `ui`, `workspaceintegration`. |
| Grafico a mano | Seleziona assi e serie con il lazo, converti, correggi punti e scala e salva. Verifica bozza modificabile e conservazione dello schizzo. Prova barre con contorno continuo e con i tre lati separati, un’annotazione isolata e tratti che potrebbero essere una seconda serie. Verifica gli errori con assi mancanti, assi a quadranti e segni ambigui. | `chartrecognition`, `ui`. Registra qualità e tempo di risposta sul Paper Pro. |
| Diagrammi manuali e UML | Inserisci forme, crea connettori e usa l’ispettore per etichette, stile, duplicazione e allineamento. Per UML verifica che i compartimenti rimangano modificabili. Esporta `.drawio`, reimporta e confronta forme e connettori. | `diagramrecognition`, `diagramsintegration`, `workspaceintegration`. |
| Riconoscimento ed estensione | Disegna la struttura, attendi il suggerimento, rivedi forme, frecce ed etichette. Scegli come trattare i tratti originali. Aggiungi una forma e un connettore ed esamina il risultato completo. Verifica conservazione di nodi precedenti e di segni ambigui e prova il ripristino. | `diagramrecognition`, `diagramcontext`, `diagramsintegration`. Prova OCR di etichette piccole e oblique con Marker reale. |

## Scambio, display e integrazioni

| Funzione | Flusso e risultato atteso | Verifica |
| --- | --- | --- |
| Quaderno portabile e USB‑C | Esporta `.quaderno` con note, formule, grafico collegato e diagramma. Importa su un profilo nuovo, verifica dati e metadati, rifiuta un file danneggiato. Prova anteprima e trasferimento in entrambi i sensi con un cavo dati. | `regression`, `sync`, `settingsintegration`. Cavo e handoff richiedono Mac e Paper Pro. |
| Wi‑Fi e Bluetooth | Abbina app sbloccate. Invia via Wi‑Fi e via Bluetooth, poi abilita la modalità automatica. Rileva lavoro via Bluetooth, verifica il passaggio a Wi‑Fi, scollega quella rete e verifica il ripiego su Bluetooth. Prova SSID consentiti, vincolo di ricarica, ripresa e rifiuto di una versione incompatibile. | `blesync`, `wirelesssync`, `syncrendezvous`, `sync`. Ripeti con radio e Wi‑Fi fisici. |
| Sincronizzazione automatica e conflitti | Seleziona uno e lascia deselezionato un altro quaderno. Prova dopo le modifiche, all’apertura e a intervalli. Inizia un lavoro con modifica su entrambi, esamina il conflitto e conserva entrambe le copie. Controlla ripresa dopo interruzione e sospensione con PIN o editor aperto. | `blesync`, `wirelesssync`, `settingsintegration`. Verifica trasporto e intervalli su due dispositivi reali. |
| Streaming e controllo remoto | Ricevi la diretta, prova sola visualizzazione, consenso e revoca. Usa clic e tastiera da Mac, ruota il tablet e ridimensiona la finestra. Interrompi, blocca o perdi il Wi‑Fi e verifica che il ricevitore smetta di mostrare fotogrammi e che l’input remoto richieda nuovo consenso. | `wirelesssync`, `workspaceintegration`. Ripeti il [collaudo del controllo remoto](COLLAUDO_CONTROLLO_REMOTO.md). |
| Ghosting durante la diretta | Con la stessa pagina e orientamento, confronta diretta spenta e attiva. Alterna per tre volte pagina con segni scuri e pagina bianca. Scrivi e cancella, apri e chiudi un pannello, ruota, esci dalla diretta e richiama il refresh manuale. La pagina bianca deve risultare leggibile, la pulizia deve attendere la fine del tratto e l’arresto deve ripristinare il profilo impostato. Registra anche latenza e residui con foto alla stessa luce e dallo stesso punto. | `workspaceintegration` per il profilo di refresh. L’immagine sul Mac documenta stream e geometria. Le foto del pannello Paper Pro documentano il ghosting. |
| Esportazione e stampa | Condividi pagina singola e intero quaderno in PDF, PNG, Markdown con risorse, CSV, draw.io e formato portabile secondo il contenuto. Verifica anteprima, tipi di file, allegati e stampa verso un PDF di prova. Invia una copia via Wi‑Fi e Bluetooth e riaprila sul Mac. | `booknotes`, `regression`, `workspaceintegration`, `wirelesssync`. Pannello di stampa e invii fisici richiedono i dispositivi. |
| Invio email | Configura un server di laboratorio TLS o STARTTLS. Invia un PDF sintetico a una casella di prova. Verifica allegato, certificato, errore con destinatario o autenticazione non validi e limiti di dimensione. | `smtp` e `workspaceintegration`. Un invio completo richiede server e casella di prova. |
| Apple | Importa copia PDF e EPUB senza DRM dell’app Libri nel Mac. Verifica consegna di appunti a Note, file per Numbers e Keynote, esportazione PDF per Libri e passaggio di testo a Diario. Verifica apertura nell’app di destinazione disponibile e copia in iCloud Drive con un account di prova. | `reader`, `ui`, `workspaceintegration` e Mac con le app effettive. |
| Obsidian | Importa Markdown UTF-8 da HTTPS, prova risposta HTML e file troppo grande. Esporta nel vault Mac e conserva testo e risorse. Verifica il collegamento Publish e, con abbonamento di prova, una sincronizzazione di testo avanti e indietro dal Paper Pro. | `reader`, `regression` e servizi esterni per Publish/Sync. |
| Google Drive, Dropbox e OneDrive | Con client OAuth registrati e account di prova, autorizza ciascun provider, elenca, importa, carica una nuova copia e scollega. Prova annullamento, revoca o scadenza. Verifica che token e originali restino fuori dai report. | `cloud`, `cloud_helper`, `settingsintegration`. L’accesso reale richiede i tre client e account. |

## Impostazioni e recupero

| Funzione | Flusso e risultato atteso | Verifica |
| --- | --- | --- |
| Venti gruppi e ricerca | Apri ogni categoria di **Impostazioni**, prova la ricerca e cambia i valori del [catalogo](IMPOSTAZIONI.md#venti-gruppi-di-impostazioni-e-relativo-effetto) in un profilo di prova. Riapri l’app e confronta valori, impostazioni reset e impostazioni indipendenti. | `tabletpreferences`, `settingsintegration`, `screenorientation`. |
| Rotazione e refresh | Prova quattro angoli, sensore automatico e orientamento fisso di un libro. Controlla allineamento di tocchi e Marker, rinvio durante modifica o finestra aperta, aggiornamento automatico e manuale. | `screenorientation`, `ui`, `settingsintegration`. Sensore, tempi e resa richiedono hardware. |
| Stato penna, luce, sospensione | Controlla Marker disponibile e distaccato, percentuale, luce e ricarica. Prova il timeout di sospensione, l’opzione durante la ricarica e il risveglio con documento aperto. | `markerbattery`, `settingsintegration`. Stati di consumo e risposta richiedono hardware. |
| Tastiera e lingua | Passa da italiano a inglese per l’interfaccia, alterna le due tastiere su schermo, abbina una tastiera Bluetooth e prova una USB. Verifica focus, accenti, cancellazione e ritorno della tastiera a schermo allo scollegamento. | `tabletpreferences`, `ui`. Serve accessorio fisico per abbinamento e scollegamento. |
| PIN e accesso SSH | Configura il PIN in un profilo di prova e controlla blocco, timeout, ripetizione errata e ripristino dell’accesso autorizzato. Su Paper Pro con accesso alternativo disponibile verifica lo stato SSH e le nuove connessioni dopo modifica della singola interfaccia. | `settingsintegration`, `paperprosshaccess`. Il controllo dei servizi reali si esegue sul dispositivo di prova. |
| Backup, migrazione e cache | Verifica backup manuale e pianificato, limite di conservazione, importazione in un profilo vuoto e recupero da indice o oggetto danneggiato. Svuota cache derivata. Confronta pagine, note, sorgenti, formule, collegamenti e preferenze locali prima e dopo. | `librarymaintenance`, `regression`, `settingsintegration`. Ripeti il percorso di recupero su due installazioni di prova. |

## Evidenze registrate

### Quaderno 3.0.0

| Ambiente | Risultato |
| --- | --- |
| macOS 27.2 su Apple Silicon, Qt 6.8.3 | Superate tutte le 24 suite CTest, senza errori o suite saltate. Avvii con rendering riusciti nei layout Mac e tablet. |
| App per Mac con libreria sintetica | Aperte le cinque sezioni e le 12 categorie di Impostazioni. Verificati cambio pagina di un PDF, disegno con Annulla e Ripeti, salvataggio di un valore negativo in tabella e inserimento di una forma. |
| Input simulato, layout tablet | Aperte le 13 categorie di Impostazioni in verticale e orizzontale. In entrambi gli orientamenti il tratto che parte dal pulsante Strumenti a pagina intera viene mantenuto. Un tocco breve apre il menu. Le regressioni includono pressione, rifiuto del palmo e gesture. |
| SDK ARM64 del Paper Pro | Compilata la 3.0.0 con l’SDK per il firmware 3.29.1.0. Avvii con rendering riusciti nei due layout con il runtime di destinazione. |
| Paper Pro fisico, firmware 3.29.1.0 | Avvio della 3.0.0 da AppLoad a 1620 × 2160. Verificati con il Marker tratti continui sia in una zona libera sia partendo dal pulsante Strumenti a pagina intera. Un tocco breve apre il menu. |
| Trascrizione su Mac | Superati sette campioni sintetici al secondo passaggio, inclusi un numero negativo e il testo su più righe. Il primo campione italiano è scaduto dopo 60 s in un avvio a freddo. |

Consegna alle app Apple, accessi reali OAuth, Obsidian e SMTP, accessori Bluetooth e ghosting E Ink richiedono prove dedicate sui dispositivi o con account di prova.

| Esecuzione | Evidenza | Stato |
| --- | --- | --- |
| Gate macOS del 22 novembre 2026 | Istantanea del sorgente di sviluppo 0.2.24, build Release su macOS 27.2 arm64 con Qt 6.8.3. Esecuzione con `run-qa.py` a 4 suite concorrenti. | **24 suite su 24 superate, 0 fallite, 0 saltate in CTest.** Tempo CTest 110,21 s. Entrambi gli avvii renderizzati superati. Risultati in `qa-results.xml` e `Testing/Temporary/LastTest.log` nella directory del build. |
| Controllo remoto con Mac e Paper Pro, 22 novembre 2026 | PDF, Post-it, Markdown, grafici, tabelle, diagrammi, navigazione in Impostazioni e arresto della condivisione dal Mac. | Verificato sui dispositivi indicati. I casi di perdita Wi‑Fi, blocco PIN e revoca sono elencati come pendenti nel relativo [registro](COLLAUDO_CONTROLLO_REMOTO.md#esiti-del-22-novembre-2026). |
| Test mirati del menu e dei riconoscimenti, 22 novembre 2026 | Gesture in verticale e orizzontale e grafico dal lazo su macOS e in VM ARM. Il motore nativo macOS ha superato 7 campioni, incluso `-5`. WritePad ARM ha superato anche il controllo sul mantenimento di `IDX01`, per un totale di 8 campioni. | Verificato in isolamento. Da ripetere con Marker e pannello fisici. |
| Griglia nativa e riconoscimento dei grafici | La griglia di prova 2×3 conserva coordinate delle celle, ordine dei tratti, celle vuote e risultati OCR mancanti. Rimossi i divisori, il riconoscimento propone due righe. La suite grafici riconosce barre tracciate in più segmenti, ignora una scritta breve separata dalla serie e rifiuta tratti ambigui. | Verificato con tratti sintetici e risultati OCR deterministici nell’editor. Da ripetere sul Paper Pro con Marker reale. |
| Servizi esterni | Simulatori e componenti locali coperti dalle suite. | Prove complete di account reali, invio SMTP e consegne Apple da registrare con account, server e app di prova. |
| Ghosting dopo la correzione dello streaming | La regressione automatica verifica il cambio di profilo all’avvio e il ripristino all’arresto. | La valutazione visiva dopo la correzione attende il Paper Pro fisico. |

Per ogni ripetizione annota versione, piattaforma, funzione, dati sintetici, risultato atteso e osservato, esito e riferimento a screenshot o log. Segna esplicitamente i casi non eseguiti, saltati dal runtime o bloccati da dispositivi e servizi. Pubblica screenshot con contenuti sintetici e senza indirizzi, token, nomi di librerie personali o credenziali.
