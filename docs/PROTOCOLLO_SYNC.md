# Sincronizzazione wireless incrementale

[English](en/SYNC_PROTOCOL.md)

`BleSync` conserva anteprima, conferma, confronto con l’ultima versione condivisa, copie di conflitto e controllo delle modifiche in corso. `DeltaTransfer` gestisce l’invio dei dati dentro lo stesso canale TLS 1.3 abbinato, su Wi-Fi locale o Bluetooth LE.

## Annotazioni dei libri e collegamenti di pagina

Evidenziazioni e sottolineature vengono salvate come tratti modificabili della pagina. Viaggiano insieme agli identificatori delle pagine, ai livelli d’inchiostro e alla loro visibilità, ai segnalibri, agli elementi di testo e alle sorgenti PDF/EPUB conservate. Il trasferimento comprende anche le annotazioni delle pagine non aperte in quel momento. La ricezione aggiorna inchiostro ed elementi visualizzati, conservando la posizione di lettura del destinatario quando quella pagina esiste ancora. Un livello nascosto resta nascosto dopo la sincronizzazione; i suoi tratti vengono conservati.

Le note ancorate introdotte nella 0.2.11 includono posizione del segnaposto, testo da tastiera, scrittura dentro la nota e colore. Usano il formato portabile **11** e richiedono lettori Quaderno compatibili su entrambi i dispositivi. Le normali evidenziazioni nei libri con sorgente conservata usano il formato 9. Consulta la [compatibilità dei formati](DATI.md).

I collegamenti ancorati introdotti nella **0.2.14** usano il formato portabile **12**. Contengono il riferimento al grafico, alla tabella o al diagramma originale. Se la destinazione è in un altro quaderno, invia anche quel quaderno oppure selezionalo separatamente per l’automatismo. Trasferisci il quaderno originale perché il collegamento possa ritrovare la destinazione tramite il suo identificatore.

Se un’annotazione non compare sull’altro dispositivo, controlla collegamento e modalità prima di importare di nuovo il libro:

1. Tieni **Quaderno aperto e sbloccato su entrambi i dispositivi**. Non esiste un servizio di sincronizzazione in background quando una delle app è chiusa.
2. Verifica che il libro corretto abbia **Sincronizza questo quaderno** selezionato nel pannello wireless. La selezione è per quaderno, non per sezione della libreria.
3. Usa **Dopo le modifiche** per avviare il trasferimento delle evidenziazioni salvate dopo la pausa di scrittura. **All’apertura di un quaderno** non si attiva a ogni annotazione successiva: riapri il libro selezionato oppure usa l’anteprima e l’invio manuali.
4. Controlla il primo trasferimento, le anteprime in attesa, i messaggi di compatibilità e i conflitti. L’ultima sincronizzazione riuscita si riferisce alla versione del quaderno confermata dall’altro dispositivo.
5. Apri la copia sincronizzata. Importare separatamente lo stesso PDF o EPUB crea un libro con un’altra identità; titoli uguali non fondono le annotazioni. Modifiche indipendenti allo stesso libro sincronizzato conservano copie separate quando entrano in conflitto.

Questa funzione trasferisce le annotazioni tra librerie di **Quaderno**. Non le inserisce nel database delle annotazioni di Apple Books. Esporta un PDF annotato quando serve una copia separata del documento.

Le nuove prove del protocollo coprono la negoziazione della versione del lettore e il rifiuto prima dell’invio delle risorse. Le prove con libri sintetici coprono evidenziazioni, sottolineature, livelli nascosti, più pagine, posizione di lettura, note e riapertura. Esecuzione e verifiche fisiche sono registrate separatamente nella validazione della versione; queste prove non dimostrano il comportamento della radio reale.

<a id="sincronizzazione-automatica-nella-029"></a>

## Sincronizzazione automatica

Il passaggio da Bluetooth a Wi-Fi richiede la 0.2.13 o successiva su entrambe le app. Completa l’abbinamento e tieni Quaderno aperto e sbloccato sul Mac e sul Paper Pro. Accendi il Bluetooth su entrambi. Collega il Paper Pro al Wi-Fi e il Mac alla stessa sottorete locale, anche via Ethernet. In **Impostazioni → Wi-Fi** oppure **Bluetooth**, imposta **Automatico (Bluetooth e Wi-Fi)** su entrambi. Su ciascun dispositivo scegli la frequenza e seleziona i quaderni che vuoi inviare. Un dispositivo può ricevere anche con la propria selezione vuota.

| Modalità | Comportamento |
| --- | --- |
| Solo manualmente | Nessun collegamento o gruppo di quaderni automatico. Resta l’impostazione predefinita. |
| Dopo le modifiche | Accorpa le modifiche salvate e attende circa 1,7 secondi senza altre modifiche prima di elaborare i quaderni selezionati. Scrittura ed editor aperti rimandano il lavoro. |
| All’apertura di un quaderno | Richiede un gruppo limitato di aggiornamenti quando apri un quaderno selezionato. Le modifiche successive attendono un nuovo evento di avvio. |
| A intervalli regolari | Richiede un gruppo limitato di aggiornamenti ogni 5, 15, 30 o 60 minuti. |

Quando attivi l’automatismo, il dispositivo si ricollega o cambia la selezione, Quaderno ricontrolla anche il lavoro pendente nei quaderni selezionati. Le richieste ripetute vengono accorpate. Versioni condivise coincidenti non richiedono un trasferimento dati; gli aggiornamenti ricevuti non devono essere subito rinviati al mittente.

### Passaggio da Bluetooth a Wi-Fi

1. Il Mac apre il collegamento Bluetooth LE e lo autentica con il segreto dell’abbinamento. Le app scambiano due indicatori booleani nel canale TLS: lavoro pendente e Wi-Fi consentito. Se le code sono vuote, resta attivo il Bluetooth in attesa di modifiche.
2. Se almeno una coda contiene lavoro e il Wi-Fi è consentito da entrambe le app, il Paper Pro comunica il suo IPv4 Wi-Fi attuale. Il Mac prova la porta TCP fissa `47987`, usando un’interfaccia Wi-Fi o Ethernet con un indirizzo nella stessa sottorete. Quaderno usa la rete esistente e non scambia credenziali, SSID, nomi dei quaderni o modifiche durante questo controllo.
3. La seconda sessione TLS deriva la chiave dal segreto di abbinamento con un’etichetta che include l’identificativo del passaggio Bluetooth. I messaggi `ready` e `commit` confermano la sessione prima di abilitare il trasferimento dati sul Wi-Fi. Il Bluetooth rimane collegato come riserva. La sessione Wi-Fi resta in uso finché è disponibile.
4. Se il passaggio fallisce o supera 6 secondi, il gruppo continua su Bluetooth. Un successivo gruppo può riprovare il Wi-Fi dopo che entrambe le code si sono stabilizzate. Se il Wi-Fi cade dopo il passaggio, il recupero riusa le parti salvate e prepara una nuova anteprima quando possibile. Un’applicazione con esito incerto richiede una revisione manuale.

I messaggi di controllo `syncRendezvous` usano metadati JSON di versione `1` e un payload vuoto. Ogni messaggio ha un campo `phase` e un `session` di 32 cifre esadecimali minuscole generato dal Mac. Le fasi `hello` e `work` includono solo i booleani `work` e `wifi` oltre ai campi comuni. `offer` aggiunge solo `address`, un IPv4 locale canonico. `ready`, `commit` e `abort` usano solo i campi comuni. Gli indirizzi USB del Paper Pro sono esclusi.

Un’app precedente risponde sul Bluetooth con il protocollo già supportato. La finestra di rilevamento del nuovo protocollo dura fino a 4 secondi. Se il collegamento Bluetooth non riesce, il Mac può provare il Wi-Fi diretto con l’indirizzo salvato e il Paper Pro può accettare il collegamento Wi-Fi compatibile mentre è in attesa. **Solo Wi-Fi**, **Solo Bluetooth** e i collegamenti avviati manualmente mantengono i propri flussi. **Disconnetti** sospende la riconnessione per la sessione e **Riprendi collegamento** la riabilita.

Gli SSID attendibili limitano il Wi-Fi automatico. Un elenco vuoto permette qualsiasi SSID locale. Se l’elenco è configurato, la rete deve corrispondere e il sistema deve poter leggere l’SSID. **Solo durante la ricarica** vale per entrambi i collegamenti sul Paper Pro. Blocco PIN, sospensione, importazione, scrittura ed editor aperti chiudono l’accesso all’automatismo. Un’applicazione già in salvataggio termina la propria operazione atomica prima di sospendere il lavoro successivo.

I tentativi di collegamento attendono inizialmente 3 secondi sul Paper Pro o 8 sul Mac, poi aumentano l’attesa fino a 120 secondi. Proseguono soltanto mentre la riconnessione automatica è consentita. Il recupero del trasferimento consente al massimo tre riprove della stessa versione, dopo 2, 4 e 8 secondi, per errori transitori riconosciuti prima dell’applicazione. Il limite sopravvive a riconnessioni e pause delle regole nell’app in esecuzione; riparte con una nuova versione sorgente, una disattivazione/riattivazione esplicita o un riavvio dell’app. Gli eventi automatici periodici o di recupero non azzerano un limite esaurito. La riprova manuale apre una nuova revisione. I gruppi programmati contengono al massimo 20 quaderni selezionati e conservano le versioni richieste tra riavvii. Nuovi eventi espliciti inseriscono in coda le ultime versioni richieste. Un punto di ripresa salvato consente al lavoro automatico interrotto prima dell’applicazione di preparare una nuova anteprima dopo la riconnessione o la riapertura. Se non può salvare il punto di ripresa prima dell’applicazione, Quaderno non invia la richiesta di applicazione. Le parti ricevute compatibili vengono riutilizzate; una conferma precedente non viene ripetuta. Annullare, disattivare l’automatismo o togliere un quaderno dalla selezione non deve lasciare una riprova attiva per quel lavoro. Un esito di applicazione incerto richiede revisione manuale. Vale anche per trasferimenti pendenti di versioni precedenti senza fase salvata o versione valida e per applicazioni manuali interrotte: il recupero automatico non deve reinviarli. Resta disponibile una nuova anteprima manuale; le verifiche finali di migrazione/ripresa sono superate su Mac e ARM (11 per piattaforma). Anche il primo trasferimento richiede revisione; i conflitti successivi seguono **Chiedi cosa fare** oppure la politica esplicitamente scelta **Conserva entrambe le copie**, che crea una copia separata senza sovrascrivere un quaderno modificato indipendentemente.

Il pannello wireless indica collegamento attivo, pausa o riprova, lavoro pendente e ultima sincronizzazione riuscita di ogni quaderno. L’invio manuale resta disponibile. Trasferimento file USB-C, Obsidian Sync e importazioni dai provider cloud mantengono i propri flussi separati: questi controlli non li trasformano in un unico servizio di sincronizzazione cloud.

## Compatibilità e dati

Le app scambiano `syncHello` e `syncHelloReply` con `deltaVersion: 1`; la risposta ripete l’identificatore di 32 caratteri della richiesta. Dalla 0.2.11 entrambi i messaggi dichiarano anche `maxNotebookVersion`, usando lo stesso limite del lettore locale. Prima di inviare manifesto o risorse, il mittente controlla la versione portabile e i campi estesi. Un lettore incompatibile non riceve contenuti del libro: il mittente indica il formato richiesto e chiede l’aggiornamento, lasciando salvate le annotazioni locali.

Un dispositivo storico con delta-v1 ma senza questo campo viene considerato compatibile con il formato 9, base comune dei lettori incrementali precedenti. Il criterio è conservativo anche per la 0.2.10, che non dichiarava il campo: aggiorna entrambe le app per scambiare le sue estensioni in formato 10 attraverso questo controllo. Una dichiarazione esplicitamente malformata viene rifiutata. Dopo 1,5 secondi senza supporto delta, il mittente passa al protocollo completo `preview`, con limite di formato 8 e i precedenti limiti di 8 MiB via Bluetooth o 64 MiB via Wi-Fi. Sorgenti dei libri e nuovi metadati non vengono rimossi o degradati per aggirare il limite.

I nuovi messaggi `deltaOffer` includono anche `notebookVersion`. Il destinatario rifiuta un formato dichiarato non supportato prima di richiedere le risorse. Dopo la ricostruzione resta obbligatoria la validazione del quaderno, anche per le offerte precedenti senza questo campo.

Il nuovo protocollo accetta un quaderno portabile fino a 64 MiB su entrambi i collegamenti. `SyncPackage` divide il JSON in descrittori di pagina e risorse immutabili identificate da SHA-256. I testi lunghi almeno 4.096 caratteri, compresi dati di immagini e sorgenti PDF/EPUB, diventano oggetti separati. Il manifesto conserva ordine delle pagine, dimensioni e hash del documento completo. Formule, tratti, testi, proprietà degli elementi e identificatori restano modificabili.

## Invio e ripresa

Il mittente invia il manifesto con `deltaOffer`. Il destinatario riusa gli oggetti già presenti nel quaderno locale o nella cache e chiede le parti mancanti con `deltaNeed`, specificando hash e posizione. `deltaChunk` invia al massimo 64 KiB con un hash di controllo. La richiesta successiva conferma la scrittura appena salvata e sincronizzata su disco. Ogni oggetto completo e il JSON ricostruito vengono verificati prima dell’anteprima.

Le parti incomplete sopravvivono alla disconnessione e al riavvio dell’app. Alla riconnessione si prepara una nuova anteprima del quaderno in attesa, riusando i dati corrispondenti. Il pulsante **Riprendi trasferimento** consente di avviare lo stesso recupero manualmente. La conferma precedente non viene ripetuta automaticamente. Le modifiche indipendenti continuano a produrre copie separate secondo il controllo dei conflitti esistente.

Una conferma di applicazione già registrata viene restituita senza applicare di nuovo lo stesso invio. Se il processo si interrompe fra il salvataggio del quaderno e quello della ricevuta, il mittente può restare incerto: deve ottenere una nuova anteprima, senza ripetere automaticamente l’applicazione.

## Cache e verifica

La cache locale `sync-objects/<identificatore-abbinamento>` ha permessi riservati all’utente. Gli oggetti inutilizzati vengono rimossi dopo 30 giorni o per riportare la cache verso 256 MiB. Durante un trasferimento lo spazio può superare questo obiettivo per la dimensione dei dati attivi. **Rimuovi abbinamento** elimina cache e preferenze del dispositivo.

`tests/sync_tests.cpp` verifica ricostruzione, librerie multiple e vuote, formule, risorse condivise, invio delle sole pagine modificate, riuso della versione locale, interruzione con ricreazione degli endpoint, dati corrotti, limiti e compatibilità precedente. I test usano cache temporanee e un collegamento simulato in processo: il comportamento fisico della radio Bluetooth richiede un collaudo sui dispositivi.

`tests/syncrendezvous_tests.cpp` verifica il formato dei messaggi di passaggio, i tipi e gli indirizzi ammessi. Le verifiche del passaggio usano collegamenti di prova. Il flusso con le radio del Mac e del Paper Pro richiede ancora un collaudo fisico.
