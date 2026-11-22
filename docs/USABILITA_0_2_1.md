# Quaderno 0.2.1: miglioramenti di usabilità

[English](en/USABILITY_0_2_1.md) · **Italiano** · [README](../README.it.md)

Questo aggiornamento in sviluppo aggiunge le 31 migliorie elencate sotto alle interfacce Mac e Paper Pro. Compilazione, avvio renderizzato e suite integrate sono stati verificati su Mac e nel runtime ARM del firmware. La versione 0.2.1 è stata anche installata su un Paper Pro fisico con firmware 3.29.1.0, con backup e verifica dell’avvio renderizzato nativo riuscita. La prova fisica di Marker, lazo e gesture resta in attesa del riscontro dell’utente.

Le [note di migrazione 0.2](AGGIORNAMENTO_0_2.md) descrivono le basi precedenti: conversione della scrittura reversibile, selezione mista di inchiostro e oggetti, riconoscimento italiano e inglese, archiviazione per pagina, caricamento asincrono dei documenti, trasferimenti wireless riprendibili, stati Apple persistenti e standby coordinato. Queste capacità erano già presenti prima dell’aggiornamento di usabilità.

## Scrittura e inserimento del testo

| N. | Miglioria | Utilizzo |
| --- | --- | --- |
| 1 | Schede Scrivi e Anteprima per Markdown | Passa dal sorgente Markdown alla visualizzazione formattata senza salvare o uscire dall’editor. L’anteprima è in sola lettura. |
| 2 | Barra di formattazione Markdown | Inserisci un titolo, grassetto, corsivo, un punto elenco o un indicatore di attività. Grassetto e corsivo racchiudono il testo selezionato; titolo ed elenchi agiscono sulla riga corrente. |
| 3 | Conteggi aggiornati del testo | Controlla il numero di parole e caratteri mentre scrivi un documento Markdown. |
| 4 | Protezione della bozza e scorciatoia Salva | Annulla, Esc e chiusura dell’app chiedono conferma prima di scartare una bozza Markdown modificata. La scorciatoia Salva della piattaforma, per esempio Cmd+S sul Mac, salva esplicitamente. |
| 5 | Navigazione del cursore e cancellazione in avanti a schermo | Spostati a sinistra o destra, raggiungi inizio o fine del testo e cancella il carattere successivo al cursore con `⌦`. Rimane disponibile il tasto backspace. Spostamento e cancellazione mantengono integri i caratteri Unicode supplementari. |
| 6 | Comandi di modifica nella tastiera a schermo | Apri **Modifica** per Seleziona tutto, Copia, Taglia, Incolla, Annulla e Ripeti. I comandi rispettano selezione, modificabilità e stato di annullamento del campo; gli appunti sono disabilitati nei campi password mascherati. |
| 7 | Tasto a capo a schermo | Usa `↵` negli editor di testo su più righe. Il tasto non compare nei campi a riga singola. |

La tastiera continua a usare i layout QWERTY italiano e inglese USA già disponibili. Il rilevamento di una tastiera fisica collegata continua a determinare se mostrare quella a schermo.

## Lazo, modelli e livelli

| N. | Miglioria | Utilizzo |
| --- | --- | --- |
| 8 | Controlli precisi per le selezioni miste | Apri **Precisione** per spostamento direzionale, variazioni di scala e rotazione a scatti, con valori numerici. Le rotazioni normali avanzano di 15°; le forme intelligenti con tratto originale conservato avanzano di 90°. |
| 9 | Reimpostazione e comandi da tastiera della selezione | **Reimposta** annulla la trasformazione in anteprima mantenendo la selezione. Le frecce spostano di un punto logico del foglio, Maiusc+frecce di dieci; Invio applica ed Esc chiude. Questi comandi locali non intercettano frecce e Invio mentre si scrive nell’editor della trascrizione. |
| 10 | Riepilogo della selezione e interruzione del riconoscimento | Leggi il numero di tratti e oggetti selezionati. **Interrompi riconoscimento** cancella una richiesta di trascrizione in corso mantenendo disponibile la selezione. |
| 11 | Anteprima del modello prima dell’applicazione | Scegli un modello per osservarne l’anteprima ingrandita, poi premi **Applica modello**. Annulla lascia invariato il modello corrente. I contenuti della pagina mantengono la loro posizione. |
| 12 | Recupero del livello attivo nascosto | La finestra Livelli spiega quando il livello attivo è nascosto e offre **Mostra livello attivo**, per riprendere a scrivere senza dover indovinare perché la penna è inattiva. |

Spostamento, scala e rotazione restano in anteprima fino ad **Applica**. Reimpostare l’anteprima non aggiunge una modifica salvata; una trasformazione applicata si può annullare. La sostituzione OCR sul foglio e il ripristino della scrittura originale appartengono già alla versione 0.2 e sono descritti nella [guida al lazo](LASSO.md).

## Tabelle e grafici

| N. | Miglioria | Utilizzo |
| --- | --- | --- |
| 13 | MIN, MAX e conteggio dei numeri | Oltre a SUM/SOMMA e AVERAGE/MEDIA, usa `MIN`, `MAX` e `COUNT`/`CONTA.NUMERI`, anche su intervalli di celle. I pulsanti rapidi inseriscono le formule in una cella vuota compatibile. |
| 14 | Coordinate delle celle | Numeri di riga, lettere di colonna ed evidenziazione della riga e colonna attive rendono più semplice individuare riferimenti come `B3`. |
| 15 | Guida alle formule ed errori comprensibili | Apri la guida per la sintassi supportata. La cella selezionata spiega riferimenti non validi, dipendenze circolari, medie senza numeri, funzioni sconosciute e argomenti non corretti. |
| 16 | Conferma prima di ridurre una tabella piena | La riduzione di righe o colonne chiede conferma quando le celle da rimuovere contengono dati. La bozza della tabella viene mantenuta in attesa della scelta. |
| 17 | Limiti negativi degli assi modificabili | I grafici calibrati manualmente espongono minimo e massimo di entrambi gli assi. Il minimo può essere negativo o zero; il massimo deve essere positivo. I valori negativi possono così essere rappresentati esplicitamente nella calibrazione e nell’esportazione CSV. |

Per esempio, con `A1 = 10`, `A2 = -5` e `A3 = 25`:

| Formula | Risultato |
| --- | --- |
| `=MIN(A1:A3)` | `-5` |
| `=MAX(A1:A3)` | `25` |
| `=COUNT(A1:A3)` oppure `=CONTA.NUMERI(A1:A3)` | `3` |

Negli intervalli testo e celle vuote vengono ignorati; gli errori delle formule si propagano. Negli esempi usa il punto decimale per evitare separatori ambigui. L’editor rimane essenziale, con un massimo di 12 righe e 8 colonne, e non offre un linguaggio completo da foglio di calcolo. I trasferimenti del quaderno conservano le formule modificabili. Un grafico collegato a una tabella nella stessa pagina usa i valori calcolati e si aggiorna quando cambiano le celle; questo collegamento precede la versione 0.2.1.

## Lettura e ricerca dei documenti

| N. | Miglioria | Utilizzo |
| --- | --- | --- |
| 18 | Navigazione per numero di pagina | La panoramica offre un campo numerico e i comandi Prima e Ultima. Sul Paper Pro è disponibile un tastierino numerico quando non è collegata una tastiera fisica. La pagina scelta viene aperta solo dopo la conferma con **Apri**. |
| 19 | Filtri per segnalibri e annotazioni | Filtra la panoramica per Tutte, Segnalibri o Annotate, con i relativi conteggi. Il riepilogo include i livelli di inchiostro nascosti e non richiede di decodificare gli sfondi per applicare il filtro. |
| 20 | Zoom di lettura con doppio tocco | Quando i gesti di lettura sono attivi, un doppio tocco ingrandisce a 2× attorno al punto toccato; un altro torna alla pagina intera. L’input del Marker non viene interpretato come questo gesto touch. |
| 21 | Nuovi ordinamenti e filtri formato della libreria | Ordina per ordine originale, titolo A–Z/Z–A o numero di pagine crescente/decrescente dove applicabile. Filtra i libri per PDF, EPUB o formato precedente/non identificato e la scrittura per Markdown o quaderni. |

I filtri formato usano la sorgente salvata del documento. Le vecchie importazioni composte solo da sfondi possono comparire come formato precedente/non identificato; questo aggiornamento non ricostruisce né reimporta automaticamente gli EPUB o PDF originali.

## Importazione sul Mac

| N. | Miglioria | Utilizzo |
| --- | --- | --- |
| 22 | Controllo prima dell’importazione dal Finder | Prima di importare, controlla la cartella di destinazione e i conteggi di file supportati, percorsi duplicati nel gruppo, file non supportati e file non leggibili. Il controllo durante il trascinamento legge i metadati locali senza analizzare ogni documento. |
| 23 | Resoconto per file e ripetizione mirata | Leggi l’esito di ciascun file e riprova solo quelli falliti senza importare nuovamente i riusciti. L’annullamento ferma il gruppo corrente/in attesa conservando i documenti già importati. |

Rimane il limite di 16 file per gruppo di importazione. Il resoconto riguarda l’ultimo gruppo, viene svuotato al blocco o alla cancellazione esplicita e non è una cronologia persistente. Il controllo dei duplicati confronta i percorsi all’interno del gruppo; non confronta tutti i libri già presenti nella libreria.

## Collegamenti, Obsidian e passaggio alle app Apple

| N. | Miglioria | Utilizzo |
| --- | --- | --- |
| 24 | Indirizzo Wi-Fi ricordato | La finestra wireless può precompilare l’ultimo indirizzo IPv4 Wi-Fi autenticato, anche dopo l’uso del Bluetooth. Il campo resta modificabile e Invio avvia il collegamento quando l’indirizzo è valido. |
| 25 | Stato di collegamento esplicito | Distingui Non abbinato, Abbinato ma disconnesso, Connessione in corso e collegamento Wi-Fi o Bluetooth autenticato. Un abbinamento salvato non viene più confuso con una connessione attiva. |
| 26 | Filtri per gli accessori Bluetooth | Sul Paper Pro filtra la ricerca per Tutti, Tastiere o Abbinati, con conteggi mostrati/totali e istruzioni in assenza di risultati. Una tastiera non classificata può comparire solo sotto Tutti. |
| 27 | Ricerca e filtri delle note Obsidian | Cerca in titoli e percorsi senza distinzione tra maiuscole e minuscole; filtra Tutte, Conflitti o Modificate con i conteggi. Il filtro non avvia un trasferimento cloud. |
| 28 | Dettagli leggibili delle consegne Apple | Le righe della coda mostrano nome della destinazione, nome/formato del file esportato e data di creazione. |
| 29 | Filtri della coda Apple | Passa tra Tutte, In attesa e Terminate per trovare le consegne ancora da completare. Terminate comprende anche quelle annullate. |
| 30 | Esportazioni conservate visibili nel Finder | Sul Mac **Mostra nel Finder** individua un’esportazione esistente dalla relativa consegna, comprese quelle completate o annullate. Le voci archiviate non hanno più una riga, ma i file restano sul disco. |
| 31 | Finestre di trasferimento adatte a spazi ridotti | La revisione USB di importazione/invio scorre nelle finestre più piccole, mostra un indicatore di attività durante il lavoro e supporta la navigazione da tastiera. Anche la finestra di condivisione Apple limita l’altezza e permette lo scorrimento. |

Il Wi-Fi continua a usare le regole di collegamento IPv4 locale già presenti e richiede di avviare anche il lato tablet sulla stessa rete. Ricordare un indirizzo non avvia automaticamente il collegamento.

Il passaggio alle app Apple continua attraverso Quaderno sul Mac: sincronizza il quaderno, scegli la destinazione e completa l’importazione nell’app Apple. La consegna viene confermata dall’utente. Questo aggiornamento non aggiunge accesso Apple Account sul Paper Pro né accesso cloud automatico a Note o Diario. [Integrazione Apple e flussi supportati](GUIDA.md#note-libri-e-icloud-sul-mac)

## Stato delle verifiche

- Il 17 novembre 2026: **6/6 suite superate su macOS** (63 casi funzionali più 12 di inizializzazione/chiusura, 17,31 s) e **6/6 suite superate su ARM** (60 casi funzionali più 12 di inizializzazione/chiusura, 14,97 s). Su ARM sono escluse come previsto tre prove riservate a macOS: conferma della consegna Apple, importazione in gruppo dal Finder e individuazione delle esportazioni nel Finder.
- I test coprono modifica della selezione, focus del testo, comandi della tastiera, annullamento/applicazione dei modelli, recupero dei livelli nascosti, formule e le altre interfacce di produzione coinvolte. Sono stati controllati anche l’avvio renderizzato nei layout desktop e tablet e le schermate dei pannelli principali. ARM usa le librerie SDK ferrari 6.0.112 / Qt 6.10.3 in una VM, non il display fisico.
- La versione 0.2.1 è stata installata sul Paper Pro fisico con firmware **3.29.1.0** tramite SSH via USB, con backup locale e sul tablet. La somma SHA del binario installato corrisponde al pacchetto. Il contenuto di tutti e quattro i libri originali è stato verificato invariato prima e dopo l’aggiunta di un quaderno di collaudo.
- L’avvio renderizzato nativo è riuscito con `QUADERNO_READY` e geometria del display **1620 × 2160**. Sono stati verificati il ritorno all’interfaccia reMarkable originale e il ripristino dello standby automatico. Dopo questi controlli Quaderno è stato avviato tramite AppLoad e osservato in esecuzione.
- Il 17 novembre 2026 l’utente ha confermato il buon esito della prova con la penna sul quaderno di collaudo: selezione con il lazo, spostamento, Applica e Annulla. Questo riscontro non verifica il riconoscimento della scrittura o le gesture di lettura. Per questo aggiornamento non sono stati eseguiti collaudo reale di accessori Bluetooth, trasferimento Wi-Fi o consegna a un’app Apple.
- Il pacchetto Mac 0.2.1 è stato verificato ed è pronto. L’aggiornamento locale è rinviato su richiesta dell’utente, che ha scelto di lasciare aperta la versione 0.2.0.
- I log locali sono `build-dev/test-usability-final.log`, `build-dev/startup-usability-final.log`, `build-arm/test-0.2.1-final.log`, `build-release/paper-pro-install-0.2.1.log` e `build-release/paper-pro-native-startup-0.2.1.log`.
- I risultati già riportati per la versione 0.2 riguardano quella revisione precedente. Consulta le [note di verifica 0.2](VERIFICA_0_2.md) per il loro ambito.

## Sviluppi successivi

Le seguenti voci sono proposte, non funzionalità implementate nell’aggiornamento:

1. **Capitoli EPUB e collegamenti interni:** un indice con accesso diretto a capitoli/ancore e cronologia per tornare indietro.
2. **Prestazioni del lazo su geometrie pesanti:** misurare selezioni con molti punti, riusare i limiti dei segmenti del poligono e ridurre i calcoli ripetuti, conservando le regole precise per elementi contenuti/toccati.
3. **Oggetti collegati tra documenti:** introdurre riferimenti stabili tra documenti e gestione esplicita dei conflitti. L’attuale collegamento dinamico tabella-grafico è locale alla stessa pagina; inserire altrove un contenuto riutilizzabile della libreria ne crea una copia.

4. **Protezione delle bozze dei diagrammi:** chiedere conferma prima di scartare le modifiche con Esc o Chiudi, come nell’editor Markdown.
5. **Connettori che evitano gli ostacoli:** far aggirare ai collegamenti ortogonali le forme estranee alla connessione.
6. **URL sorgente del Markdown:** conservare l’indirizzo originale dei documenti web per risolvere collegamenti e immagini relativi.
