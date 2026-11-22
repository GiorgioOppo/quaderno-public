# Backup locali, cache e preferenze di lettura

**Versione:** 0.2.7 in sviluppo. [English](en/LIBRARY_MAINTENANCE.md).

Queste impostazioni riguardano il dispositivo in uso. Un backup creato sul Paper Pro resta sul Paper Pro: la creazione non lo trasferisce al Mac e non lo carica su un account cloud.

## Creare un backup della libreria

Apri i controlli dei backup nelle Impostazioni e scegli **Crea backup adesso**. L’operazione avviene in background. Prima di copiare il risultato altrove, attendi il messaggio che conferma il salvataggio locale.

Puoi scegliere backup manuali, giornalieri o settimanali e conservare 1, 5, 10 o 20 versioni. L’impostazione iniziale è manuale; il numero predefinito di versioni è 5. La pianificazione funziona solo mentre Quaderno è aperto e la manutenzione è consentita. L’app sospende l’avvio di nuovi backup quando è bloccata, a riposo, durante un’importazione o nell’uso di un editor. Un intervallo scaduto viene controllato quando l’app torna disponibile: non viene creato un servizio di sistema che avvia automaticamente Quaderno.

Ogni copia contiene la libreria salvata completa: libri, documenti originali incorporati, scrittura, annotazioni, tabelle, formule, grafici, cartelle e posizioni di lettura. Quando presenti, conserva separatamente anche le preferenze locali di lettura dei singoli libri. Credenziali degli account, configurazione del PIN e preferenze generali del dispositivo sono escluse.

Il backup viene reso disponibile solo dopo il salvataggio corretto dei suoi file. Le vecchie versioni possono essere eliminate soltanto dopo la riuscita di una nuova copia e con un’impostazione di conservazione applicata. Prima della rimozione vengono verificati appartenenza, hash e contenuto. Copie modificate, file estranei, collegamenti simbolici e cartelle con file aggiuntivi restano conservati. Il lavoro di pulizia è limitato: un archivio insolitamente grande può mantenere temporaneamente più versioni del numero scelto. Un backup fallito non avvia l’eliminazione delle vecchie copie.

Conserva una copia anche su un altro dispositivo o supporto per proteggerti dalla perdita del tablet. La pianificazione dei backup locali e la sincronizzazione wireless dei quaderni sono funzioni distinte.

## Recuperare una libreria salvata

Ogni copia si trova accanto al file della libreria:

```text
<file-libreria>.backups/snapshot-<UUID>/
    library.quaderno
    metadata.json
    reader-settings.json   # presente se esistono preferenze locali dei libri
```

Copia la cartella del backup prima di spostarne o modificarne i file. Su macOS, **Importa quaderni** accetta `library.quaderno` e i file JSON di Quaderno. L’importazione usa le regole di unione già presenti nell’app e non sostituisce indiscriminatamente la libreria corrente. Le modifiche divergenti possono generare copie separate in conflitto.

L’importazione esplicita da un file locale accetta fino a 256 MiB, coerentemente con il limite della libreria locale. Gli ingressi di rete e i trasferimenti portabili mantengono il limite separato di 64 MiB. Per un backup locale grande può quindi essere necessaria una copia del file anziché il trasferimento wireless dell’intera libreria.

L’importazione ordinaria ripristina i documenti senza applicare a un altro dispositivo il file separato `reader-settings.json`. Questo file viene conservato per il recupero locale; non esistono un ripristino automatico di account/PIN o un comando unico per ripristinare il file delle preferenze di lettura.

## Controllare e svuotare la cache

I controlli dello spazio distinguono il file principale della libreria, gli oggetti immutabili che contengono i documenti, le copie ricreabili dei documenti e i backup. **Aggiorna spazio** aggiorna i dati in background. Gli originali e le annotazioni appartengono all’archivio della libreria, non alla cache eliminabile.

Le dimensioni disponibili sono 32, 64, 128 e 256 MiB. Il valore limita la cache condivisa delle pagine renderizzate in memoria; lo stesso limite viene applicato separatamente alle copie PDF decodificate su disco durante la lettura. Non è un limite alla memoria complessiva dell’app: anche la pagina visibile, le elaborazioni in corso e i quaderni occupano memoria.

**Svuota cache** elimina le copie riconosciute e verificate dei documenti e svuota le immagini riutilizzabili delle pagine. Conserva originali incorporati, tratti, annotazioni, formule, impostazioni locali di lettura e backup. La successiva pagina richiesta viene ricostruita dall’originale archiviato. I file non riconosciuti e i collegamenti simbolici restano intatti. Se una vecchia cache molto grande supera il lavoro consentito per una singola operazione, il messaggio chiede di ripetere la pulizia.

## Preferenze di lettura per ciascun libro

Le impostazioni generali del lettore forniscono i valori predefiniti. Ogni libro può scegliere zoom, margine, orientamento e comportamento dello scorrimento senza modificare gli altri libri. Il ripristino delle preferenze del singolo libro torna ai valori generali.

| Preferenza | Valori locali del libro |
| --- | --- |
| Zoom | Percentuali intere dal 100% al 300%, incluso lo zoom salvato dopo una gesture |
| Margine | Nessuno, piccolo, medio o grande |
| Orientamento | Predefinito dell’app, verticale, orizzontale a destra, verticale capovolto o orizzontale a sinistra |
| Scorrimento | Cambio pagina, scorrimento verticale o nessuna azione |

Le scelte vengono salvate in `<file-libreria>.reader-settings.json`, separatamente dai quaderni. Rimangono dopo il riavvio dell’app e dopo una sincronizzazione sullo stesso dispositivo. Non modificano la revisione usata per il trasferimento del quaderno e non vengono inviate all’altro dispositivo. Un aggiornamento parziale conserva le altre scelte del libro. Un file danneggiato o modificato da un’altra istanza non viene sovrascritto in silenzio; i quaderni restano modificabili.

## Implementazione e verifiche

`LibraryMaintenance` espone le operazioni asincrone `backup()`, `clearCache()` e `refreshStatus()`. La proprietà `allowed` controlla le modifiche e gli avvii pianificati. Un blocco dedicato serializza le operazioni di manutenzione. I backup leggono un file principale atomico e verificano gli oggetti immutabili senza occupare il blocco di salvataggio dei quaderni: se una pulizia concorrente rimuove un oggetto necessario, il backup fallisce senza impedire il salvataggio della scrittura.

I test automatici coprono importazione e conservazione dei backup, file manomessi, limiti e ricostruzione della cache, preferenze locali dei libri, condizioni della pianificazione, conservazione dei tratti durante l’inserimento del testo OCR e separazione dei limiti di importazione locale e di rete. Il collaudo fisico della UI e dell’installazione 0.2.7 viene riportato separatamente nei registri di verifica della versione.
