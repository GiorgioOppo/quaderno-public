# Sincronizzazione wireless incrementale

[English](en/SYNC_PROTOCOL.md)

`BleSync` conserva anteprima, conferma, confronto con l’ultima versione condivisa, copie di conflitto e controllo delle modifiche in corso. `DeltaTransfer` gestisce l’invio dei dati dentro lo stesso canale TLS 1.3 abbinato, su Wi-Fi locale o Bluetooth LE.

## Annotazioni dei libri e compatibilità nella 0.2.11

Evidenziazioni e sottolineature vengono salvate come tratti modificabili della pagina. Viaggiano insieme agli identificatori delle pagine, ai livelli d’inchiostro e alla loro visibilità, ai segnalibri, agli elementi di testo e alle sorgenti PDF/EPUB conservate. Il trasferimento comprende anche le annotazioni delle pagine non aperte in quel momento. La ricezione aggiorna inchiostro ed elementi visualizzati, conservando la posizione di lettura del destinatario quando quella pagina esiste ancora. Un livello nascosto resta nascosto dopo la sincronizzazione; i suoi tratti vengono conservati.

Le note ancorate introdotte nella 0.2.11 includono posizione del segnaposto, testo da tastiera, scrittura dentro la nota e colore. Usano il formato portabile **11** e richiedono lettori Quaderno compatibili su entrambi i dispositivi. Le normali evidenziazioni nei libri con sorgente conservata restano in formato 9; un’annotazione richiede il formato 11 soltanto se usa il nuovo tipo di nota. Consulta la [compatibilità dei formati](DATI.md).

Se un’annotazione non compare sull’altro dispositivo, controlla collegamento e modalità prima di importare di nuovo il libro:

1. Tieni **Quaderno aperto e sbloccato su entrambi i dispositivi**. Non esiste un servizio di sincronizzazione in background quando una delle app è chiusa.
2. Verifica che il libro corretto abbia **Sincronizza questo quaderno** selezionato nel pannello wireless. La selezione è per quaderno, non per sezione della libreria.
3. Usa **Dopo le modifiche** per avviare il trasferimento delle evidenziazioni salvate dopo la pausa di scrittura. **All’apertura di un quaderno** non si attiva a ogni annotazione successiva: riapri il libro selezionato oppure usa l’anteprima e l’invio manuali.
4. Controlla il primo trasferimento, le anteprime in attesa, i messaggi di compatibilità e i conflitti. L’ultima sincronizzazione riuscita si riferisce alla versione del quaderno confermata dall’altro dispositivo.
5. Apri la copia sincronizzata. Importare separatamente lo stesso PDF o EPUB crea un libro con un’altra identità; titoli uguali non fondono le annotazioni. Modifiche indipendenti allo stesso libro sincronizzato conservano copie separate quando entrano in conflitto.

Questa funzione trasferisce le annotazioni tra librerie di **Quaderno**. Non le inserisce nel database delle annotazioni di Apple Books. Esporta un PDF annotato quando serve una copia separata del documento.

Le nuove prove del protocollo coprono la negoziazione della versione del lettore e il rifiuto prima dell’invio delle risorse. Le prove con libri sintetici coprono evidenziazioni, sottolineature, livelli nascosti, più pagine, posizione di lettura, note e riapertura. Esecuzione e verifiche fisiche sono registrate separatamente nella validazione della versione; queste prove non dimostrano il comportamento della radio reale.

## Sincronizzazione automatica nella 0.2.9

Il flusso della 0.2.9 in sviluppo richiede entrambe le app Quaderno aggiornate, aperte e sbloccate, un abbinamento salvato e quaderni selezionati per la sincronizzazione. Non opera quando Quaderno è chiuso o sospeso e non usa un inoltro Internet o un account cloud. Sono superati build Mac e ARM, sei suite interessate per piattaforma e avvii rigorosi renderizzati. La 0.2.9 è installata sul Paper Pro e ha superato l’avvio nativo e-paper/AppLoad a 1620 × 2160. L’utente ha confermato la visibilità della modalità dopo le modifiche e delle tre scelte di collegamento sul Paper Pro. I trasferimenti Wi-Fi/Bluetooth completi tra dispositivi restano da provare; l’app Mac 0.2.0 installata resta invariata. Il pacchetto Mac 0.2.9 separato è pronto, ma non ha sostituito quell’installazione.

In **Impostazioni → Wi-Fi** oppure **Bluetooth**, apri **Sincronizzazione automatica** e scegli quando inviare le modifiche:

| Modalità | Comportamento |
| --- | --- |
| Solo manualmente | Nessun collegamento o gruppo di quaderni automatico. Resta l’impostazione predefinita. |
| Dopo le modifiche | Accorpa le modifiche salvate e attende circa 1,7 secondi senza altre modifiche prima di elaborare i quaderni selezionati. Scrittura ed editor aperti rimandano il lavoro. |
| All’apertura di un quaderno | Richiede un gruppo limitato di aggiornamenti quando apri un quaderno selezionato. Le modifiche successive attendono un nuovo evento di avvio. |
| A intervalli regolari | Richiede un gruppo limitato di aggiornamenti ogni 5, 15, 30 o 60 minuti. |

Quando attivi l’automatismo, il dispositivo si ricollega o cambia la selezione, Quaderno ricontrolla anche il lavoro pendente nei quaderni selezionati. Le richieste ripetute vengono accorpate. Versioni condivise coincidenti non richiedono un trasferimento dati; gli aggiornamenti ricevuti non devono essere subito rinviati al mittente.

Scegli **Preferisci Wi-Fi, altrimenti Bluetooth**, **Solo Wi-Fi** oppure **Solo Bluetooth**. La scelta automatica preferisce il Wi-Fi locale consentito e può provare il dispositivo Bluetooth abbinato se il Wi-Fi non è disponibile. Sul Paper Pro la modalità automatica può restare in ascolto su entrambi i collegamenti consentiti, con Bluetooth già acceso, e selezionare il primo accettato. Sul Mac prova prima il Wi-Fi e può tentare il Bluetooth dopo un errore Wi-Fi. Un collegamento autenticato viene conservato finché termina, anche se in seguito il Wi-Fi torna disponibile; la preferenza automatica non lo interrompe per cambiare trasporto. Sul Mac il Wi-Fi usa l’indirizzo IPv4 locale salvato; se cambia l’indirizzo del tablet può ancora servire correggerlo nel pannello del collegamento. Il Bluetooth deve essere già acceso: il recupero automatico non attiva una radio disabilitata. **Disconnetti** sospende la riconnessione per la sessione; **Riprendi collegamento** la consente di nuovo.

Gli SSID attendibili limitano soltanto il Wi-Fi automatico, anche quando è il collegamento preferito. Un elenco vuoto non impone vincoli; un elenco configurato con SSID diverso o non disponibile impedisce il Wi-Fi automatico. Il Bluetooth continua a richiedere l’abbinamento e il consenso della modalità di collegamento scelta. **Solo durante la ricarica** vale per entrambi i collegamenti sul Paper Pro. Blocco PIN, sospensione, importazione, scrittura ed editor aperti chiudono subito l’accesso all’automatismo. Un’applicazione già in salvataggio termina la propria operazione atomica prima di sospendere il lavoro successivo.

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
