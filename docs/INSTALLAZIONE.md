# Installare Quaderno

[English](en/INSTALLATION.md) · **Italiano**

Quaderno gira su Mac e, in una sessione di prova, sul reMarkable Paper Pro. La build per tablet è compilata per il modello `ferrari` con firmware **3.29.1.0**. Su un tablet fisico sono stati verificati l’avvio da AppLoad, gli eventi nativi del Marker e il refresh regionale e manuale. [Stato del rilascio e del collaudo](RILASCIO.md)

| Destinazione | Pacchetto | Requisito |
| --- | --- | --- |
| Mac | `Quaderno-macOS.zip` | Mac con Apple Silicon e macOS 27.2 o successivo per l’anteprima attuale. La funzione di trascrizione richiede macOS 27 anche nelle build dai sorgenti. |
| reMarkable Paper Pro | `Quaderno-Paper-Pro-3.29.1.0.zip` | Modello `ferrari`, firmware 3.29.1.0 e modalità sviluppatore. |

Se hai già il pacchetto della tua piattaforma, segui le istruzioni qui sotto. Per generarlo dai sorgenti, consulta la [guida di sviluppo e compilazione](SVILUPPO.md).

## macOS

1. Estrai `Quaderno-macOS.zip` nel Finder.
2. Sposta `Quaderno.app` in **Applicazioni** e aprila.
3. Crea un quaderno di prova e verifica che si riapra dopo aver chiuso l’app.

L’anteprima Mac è firmata localmente, con firma ad hoc. Se macOS segnala che lo sviluppatore è sconosciuto, controlla la provenienza dello ZIP. Apple documenta come autorizzare l’apertura di una singola app da **Impostazioni di Sistema → Privacy e sicurezza**. [Aprire le app in sicurezza sul Mac](https://support.apple.com/en-au/102445)

Per aggiornare, chiudi Quaderno e sostituisci `Quaderno.app` in **Applicazioni**. Il pacchetto dell’app e la libreria sono in cartelle diverse. Per il backup di `quaderni.json` e delle impostazioni, consulta la [guida ai dati](DATI.md).

## Paper Pro via USB‑C

### Preparare il tablet

> [!IMPORTANT]
> L’attivazione della modalità sviluppatore ripristina il Paper Pro alle impostazioni di fabbrica e riduce le protezioni del sistema. Prima di attivarla, sincronizza o esporta i documenti e verifica di poter aprire le copie. Per disattivarla in seguito serve il ripristino ufficiale. [Modalità sviluppatore reMarkable](https://developer.remarkable.com/documentation/developer-mode)

1. Verifica il modello e la versione completa del software del tablet prima di eseguire l’installer. Questa guida usa il pacchetto per **reMarkable Paper Pro 3.29.1.0**. Lo script verifica modello, architettura e dipendenze.
2. Se la modalità sviluppatore è già attiva, passa al collegamento USB. Per attivarla, la guida reMarkable indica **Impostazioni → Generali → Paper Tablet → Software → Avanzate → Modalità sviluppatore**. Le etichette possono variare con la lingua del sistema. [Attivazione della modalità sviluppatore](https://developer.remarkable.com/documentation/developer-mode#enabling-developer-mode)
3. A tablet acceso e sbloccato, collegalo al Mac con un cavo USB‑C che trasferisce dati. Sul Paper Pro trovi le credenziali SSH in **Impostazioni → Generali → Guida → Informazioni → Copyright e licenze**. Usa la password SSH indicata lì. [Accesso SSH in modalità sviluppatore](https://developer.remarkable.com/documentation/developer-mode#using-developer-mode)

Dal Mac controlla il collegamento. Al primo accesso SSH mostra l’impronta del dispositivo. Verifica l’identità del tablet prima di accettarla.

```sh
ssh root@10.11.99.1
```

Torna al terminale del Mac con `exit`.

### Installare e avviare

Estrai `Quaderno-Paper-Pro-3.29.1.0.zip` e apri il Terminale nella cartella estratta. Tieni `quaderno` e gli script nella stessa cartella.

Per attivare la trascrizione sul tablet, tieni `quaderno-handwriting` e `quaderno-handwriting-it.dct` accanto a `quaderno`. L’installatore trasferisce e verifica entrambi i file quando sono presenti.

```sh
sh install-paper-pro.sh
```

Quando SSH chiede la password, inseriscila nel Terminale. Durante la digitazione il Terminale può lasciare il cursore fermo. Lo script trasferisce i file, verifica il caricatore AArch64, le librerie e i plugin e‑ink, poi stampa il percorso dell’installazione e il comando per riavviare la prova.

Per installare ed eseguire le verifiche mantenendo aperta l’interfaccia reMarkable, usa questa opzione.

```sh
sh install-paper-pro.sh --install-only
```

Ogni installazione riuscita crea una cartella `/home/root/quaderno/installation-…` sulla partizione dati. Conserva il comando stampato dall’installer per riaprire quella versione. Sul tablet, `try-on-device.sh --check` verifica i prerequisiti della prova e `try-on-device.sh --stop` arresta la sessione attiva.

### Collegare Obsidian Sync

Per la sincronizzazione diretta serve un abbonamento Obsidian Sync attivo e un vault remoto già configurato. Ogni Paper Pro si collega a un solo vault, di cui conserva una copia locale separata. Fai una copia di sicurezza del vault prima di collegare il tablet. Quaderno usa il client ufficiale Obsidian Headless, attualmente in beta. [Documentazione Headless Sync](https://obsidian.md/help/sync/headless)

1. Installa la nuova build di Quaderno. Collega il Paper Pro via USB‑C e connettilo al Wi‑Fi con accesso a Internet. Dal Terminale sul Mac, nella cartella del pacchetto estratto, esegui questo comando. L’installer scarica separatamente Node.js, Obsidian Headless e le relative dipendenze, quindi verifica le impronte prima di trasferirli sul tablet.

   ```sh
   sh obsidian-sync-paper-pro.sh install
   ```

2. Sempre nel Terminale, esegui la configurazione interattiva. Inserisci le credenziali Obsidian e l’eventuale codice a due fattori quando li chiede il client. Scegli il vault remoto tramite l’identificativo mostrato. Per un vault cifrato end-to-end, inserisci anche la password del vault quando viene richiesta. Se conosci già l’identificativo, puoi passarlo come argomento dopo `setup`.

   ```sh
   sh obsidian-sync-paper-pro.sh setup
   ```

3. Avvia Quaderno sul tablet e apri **Markdown e Obsidian → Obsidian Sync → Sincronizza ora**. Completata la configurazione, gli scambi con il vault usano il Wi‑Fi del Paper Pro. [Guida alla sincronizzazione](GUIDA.md#obsidian-sync-sul-paper-pro)

Obsidian Headless 0.0.14 può chiedere una password anche per un vault con crittografia gestita, che non ha una password di cifratura propria. [Segnalazione nel repository Obsidian Headless](https://github.com/obsidianmd/obsidian-headless/issues/51)

### Tornare a reMarkable

Premi **Esci** in Quaderno. La prova usa il servizio temporaneo `quaderno-demo.service`, che ferma `xochitl` durante la sessione e ne richiede il riavvio quando Quaderno termina. La sessione dura fino a dieci minuti. Una disconnessione USB lascia il servizio in esecuzione fino alla chiusura dell’app o alla scadenza.

Per terminare la prova dal Mac, ricollega il tablet se necessario ed esegui il comando seguente.

```sh
ssh root@10.11.99.1 'systemctl stop quaderno-demo.service'
```

Se al termine la schermata reMarkable resta assente e SSH risponde, controlla i due servizi. Avvia `xochitl` dopo aver verificato che la prova sia terminata.

```sh
ssh root@10.11.99.1 'systemctl status quaderno-demo.service xochitl.service --no-pager'
ssh root@10.11.99.1 'systemctl start xochitl.service'
```

Se il tablet non si avvia o vuoi uscire dalla modalità sviluppatore, segui il ripristino ufficiale per il Paper Pro. Prima del ripristino salva i documenti ancora accessibili. [Recovery Mode reMarkable](https://developer.remarkable.com/documentation/recovery-mode)

### Avviare dal menu AppLoad

Questo percorso richiede XOVI e AppLoad già installati e funzionanti sul tablet. Quaderno registra la propria voce nel menu. Prima di configurare le estensioni, consulta la loro documentazione per il tuo modello e firmware. [XOVI](https://github.com/asivery/rm-xovi-extensions#to-install-xovi) · [AppLoad](https://github.com/asivery/rm-appload#external-applications)

Dalla cartella del pacchetto sul Mac esegui il comando seguente.

```sh
sh install-paper-pro.sh --appload
```

Sul tablet, apri **AppLoad**, premi **Reload** se il menu era già aperto e scegli **Quaderno**. Si avvia la stessa sessione temporanea usata via USB. La voce funziona mentre XOVI è attivo. Se usi XOVI in modalità temporanea, riattivalo dopo il riavvio del tablet.

Per togliere la voce usa il comando di rimozione stampato dall’installer. Poi premi **Reload** in AppLoad. La rimozione conserva il binario installato e i quaderni.

### Aggiornare o riaprire la build precedente

1. Chiudi la sessione di prova e fai una copia della libreria seguendo la [guida ai dati](DATI.md).
2. Controlla il firmware e usa il pacchetto corrispondente. Installa la nuova build con `--install-only` per provarla via USB o con `--appload` per aggiornare anche la voce del menu.
3. Conserva la vecchia cartella di installazione e il suo comando di avvio finché la nuova build funziona. Per riaprire la precedente, usa il comando stampato dalla sua installazione. Per ricollegarla ad AppLoad, usa `install-appload-quaderno.sh` dalla cartella installata con `--appload` e passagli il percorso completo della vecchia cartella `installation-…` sul tablet.

Prima di riaprire una libreria con una build precedente, verifica che legga il formato usato dalla versione nuova. Conserva una copia separata della libreria aggiornata se decidi di ripristinare il backup precedente.

### Rimuovere Quaderno

Su Mac chiudi Quaderno e sposta `Quaderno.app` nel Cestino. Sul Paper Pro termina la prova e, se presente, rimuovi la voce AppLoad con il comando stampato dall’installer. Puoi quindi cancellare la cartella della build in `/home/root/quaderno/installation-…`. La libreria vive nella cartella dati descritta nella [guida ai dati](DATI.md).

## Risolvere i problemi

| Sintomo | Controllo |
| --- | --- |
| SSH va in timeout | Usa un cavo dati, prova un’altra porta USB e controlla che il tablet sia acceso e sbloccato. Verifica la modalità sviluppatore e riprova con `ssh root@10.11.99.1`. |
| SSH rifiuta l’accesso | Controlla la password mostrata in **Copyright e licenze** sul Paper Pro. |
| SSH segnala un’identità cambiata | Verifica che all’indirizzo USB risponda il tuo tablet e controlla se è stato ripristinato. Riprendi il collegamento dopo aver verificato la nuova impronta. |
| La verifica delle librerie o dei plugin e‑ink fallisce | Controlla firmware, modello e integrità dello ZIP. Per un altro firmware compila con l’SDK corrispondente e verifica quella combinazione prima della prova. |
| AppLoad mostra la voce vecchia o non mostra Quaderno | Controlla che XOVI e AppLoad siano attivi, riesegui l’installer con `--appload` e premi **Reload**. |
| Quaderno si chiude sul tablet | Consulta il log della sessione. Per un avvio da AppLoad controlla anche il servizio di avvio. |

```sh
ssh root@10.11.99.1 'journalctl -b -u quaderno-demo.service -u quaderno-launch.service -n 80 --no-pager'
```

Per continuare dopo l’installazione, vai alla [guida utente](GUIDA.md).
