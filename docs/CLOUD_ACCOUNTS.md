# Account cloud

[English](en/CLOUD_ACCOUNTS.md)

Quaderno 0.2.7 comprende collegamenti diretti a Dropbox, Google Drive e OneDrive. Il pannello account offre **Accedi con Google**, **Accedi con Dropbox** e **Accedi con Microsoft**. La password dell’account si inserisce soltanto nella pagina ufficiale del fornitore.

Il codice di collegamento è implementato e verificato con test di protocollo offline. **Non sono incluse registrazioni OAuth di produzione e durante questo sviluppo non è stato eseguito l’accesso ad account reali.** Un manutentore deve fornire le registrazioni di proprietà del progetto prima che i relativi pulsanti SSO possano autenticare. Configurazione assente, archivio privato inaccessibile ed errori di autorizzazione sono mostrati esplicitamente; aprire un sito non equivale ad aver collegato l’account.

## Funzioni disponibili

- Esplorazione delle cartelle e caricamento delle pagine successive, fino a 2.000 elementi nella cartella corrente.
- Download HTTPS di PDF, EPUB, Markdown, CSV, PNG, JPEG, WebP e `.quaderno`, seguito dalla normale importazione. I quaderni portabili richiedono anteprima e conferma.
- Caricamento di un file locale su macOS o esportazione del quaderno corrente come nuova copia `.quaderno` su entrambi i dispositivi.
- Rinnovo dei token in scadenza, rimozione dell’accesso da questo dispositivo e revoca presso il fornitore dove disponibile.
- Annullamento delle operazioni. Il blocco PIN interrompe anche le richieste, chiude il ritorno OAuth locale e cancella codici temporanei ed elenco dei file.

I trasferimenti sono operazioni esplicite, non una sincronizzazione automatica bidirezionale. Non vengono eliminati file cloud. Dropbox aggiunge una copia eventualmente rinominata, Google crea un nuovo file e OneDrive rifiuta i conflitti di nome. Quaderno non unisce modifiche remote e non sostituisce gli originali.

Il limite è **100 MiB per file**, oppure **64 MiB per `.quaderno`**. Restano validi i limiti dei singoli importatori. I documenti nativi Google Workspace non vengono convertiti o esportati da questo collegamento: occorre prima ottenerli in un formato supportato. Quota disponibile, criteri aziendali, permessi di condivisione e disponibilità del servizio possono limitare le operazioni.

## Configurazione del manutentore

La registrazione dell’app è separata dal normale accesso dell’utente. Non usare identificativi appartenenti ad altre app, non aggiungere token al repository e non distribuire il segreto di un’applicazione riservata in un programma desktop.

### Google Drive

Crea un progetto Google Cloud, abilita Drive API, configura la schermata di consenso OAuth e registra un client di tipo **Desktop app**. Quaderno usa authorization code con PKCE S256 e un ritorno temporaneo `http://127.0.0.1:<porta>/oauth/callback`. Non usa il vecchio flusso Google di copia/incolla del codice. Il campo facoltativo `clientSecret` riguarda soltanto il client Desktop corrispondente, se necessario. [Documentazione OAuth desktop Google](https://developers.google.com/identity/protocols/oauth2/native-app).

Gli scope richiesti sono `https://www.googleapis.com/auth/drive.readonly` e `https://www.googleapis.com/auth/drive.file`: lettura dei file accessibili esistenti e creazione di copie appartenenti all’app. La lettura generale è uno scope ristretto e la distribuzione pubblica può richiedere la verifica di Google. Configura utenti di prova e stato di pubblicazione prima di effettuare l’accesso. Il device flow Google non offre la stessa esplorazione dei file esistenti; sul tablet viene quindi usato il ritorno desktop attraverso SSH. [Scope Drive](https://developers.google.com/workspace/drive/api/guides/api-specific-auth), [limiti del device flow](https://developers.google.com/identity/protocols/oauth2/limited-input-device).

### Dropbox

Registra un’app con scope nella [console sviluppatori Dropbox](https://www.dropbox.com/developers/apps), abilita i client pubblici e i permessi `files.metadata.read`, `files.content.read` e `files.content.write`. Scegli Full Dropbox per leggere i file esistenti esterni alla cartella dell’app; una registrazione App Folder rende visibile solo quella cartella.

Quaderno usa la chiave pubblica come `clientId`, PKCE S256 e `token_access_type=offline`. Omettendo `redirect_uri`, Dropbox mostra un codice monouso: inseriscilo in Quaderno oppure nel prompt nascosto dell’assistente SSH sul Mac. Non viene utilizzato il segreto dell’app Dropbox. [Guida OAuth Dropbox](https://docs.dropboxapi.com/dropbox-api/docs/oauth).

### Microsoft OneDrive

Registra un’app in Microsoft Entra, seleziona i tipi di account da supportare e abilita **Allow public client flows**. Serve l’Application/client ID, senza client secret. Il tenant può essere `common`, `consumers`, `organizations` oppure il GUID della directory. Gli scope delegati richiesti sono `Files.ReadWrite` e `offline_access`.

Quaderno ottiene un device code, mostra il codice utente e attende la conferma nella pagina Microsoft, rispettando intervallo di polling, rallentamenti e scadenza. Se un criterio aziendale vieta questo metodo, l’errore viene mostrato senza aggirarlo. [Device flow Microsoft](https://learn.microsoft.com/en-us/entra/identity-platform/v2-oauth2-device-code), [configurazione del client pubblico](https://learn.microsoft.com/en-us/entra/identity-platform/scenario-desktop-app-configuration).

## Installare la registrazione

Prepara un JSON privato contenente soltanto i fornitori configurati. Sostituisci i segnaposto con le registrazioni di tua proprietà:

```json
{
  "googleDrive": {
    "clientId": "YOUR_DESKTOP_CLIENT_ID.apps.googleusercontent.com"
  },
  "dropbox": {
    "clientId": "YOUR_DROPBOX_PUBLIC_APP_KEY"
  },
  "oneDrive": {
    "clientId": "YOUR_MICROSOFT_APPLICATION_ID",
    "tenant": "common"
  }
}
```

Il file di destinazione è `oauth-registration.json` dentro `QStandardPaths::AppConfigLocation/cloud`. Sul Paper Pro il percorso è `/home/root/.config/Quaderno/cloud/`; su macOS normalmente è `~/Library/Preferences/Quaderno/cloud/`. La directory deve avere permessi `0700`, il file `0600`. Il normale pannello account non chiede all’utente di compilare questi dati da sviluppatore.

Dal Mac, con SSH abilitato sul tablet:

```sh
chmod 600 /percorso/privato/oauth-registration.json
python3 scripts/cloud-auth-paper-pro.py --host 10.11.99.1 \
  --registration /percorso/privato/oauth-registration.json
```

Per la configurazione wireless usa l’indirizzo Wi-Fi privato del tablet al posto di `10.11.99.1`. La verifica della chiave host SSH rimane attiva. Inserisci l’eventuale password SSH nel Terminale, mai in chat. L’assistente controlla che il dispositivo remoto si identifichi come Paper Pro prima di scrivere la configurazione. Poi riapri il pannello dell’account.

Su macOS installa lo stesso JSON, con permessi privati, nella directory di configurazione locale. Installare la registrazione non avvia automaticamente l’accesso al fornitore. Non inserire password degli account nel JSON.

## Accedere dal Paper Pro

1. Apri Impostazioni → Account, scegli un servizio configurato e premi il suo pulsante SSO.
2. Sul Mac avvia esplicitamente l’assistente:

   ```sh
   python3 scripts/cloud-auth-paper-pro.py --host 10.11.99.1
   ```

3. Completa la pagina ufficiale nel browser. Con Microsoft inserisci il codice mostrato dal Paper Pro. Con Dropbox incolla il codice di autorizzazione nel prompt nascosto del Terminale oppure nel campo sul tablet.
4. Controlla l’esito e l’elenco dei file in Quaderno. La chiusura della richiesta nell’assistente può indicare anche annullamento o errore, non necessariamente accesso riuscito.

Per Google il collegamento SSH temporaneo inoltra il ritorno localhost del Mac al listener localhost del tablet. Il Paper Pro scambia il codice con Google e conserva localmente i token. L’assistente legge solo metadati di autorizzazione in scadenza, mai access token o refresh token. Aprire soltanto l’URL Google del tablet da un altro dispositivo, senza inoltro, non completa il ritorno.

L’opzione `--control /percorso/socket` riutilizza una connessione SSH già autenticata. L’assistente rimuove solo il proprio inoltro e non chiude un master SSH preesistente. Dopo l’autenticazione, i trasferimenti cloud avvengono direttamente via HTTPS dal Paper Pro, senza Mac o cavo.

Su macOS il pulsante SSO apre direttamente il browser ufficiale: Google ritorna all’app locale, Dropbox fornisce il codice e Microsoft usa il device code mostrato.

## Archiviazione, blocco e revoca

Su macOS i token sono nel Portachiavi di Quaderno. Le query non aprono finestre di autenticazione all’avvio: se l’archivio non è accessibile, l’app mostra un errore e non ripiega su un file in chiaro. Sul Paper Pro i token vengono salvati atomicamente in un file `0600` nella directory `0700`. Questi permessi limitano l’accesso degli altri utenti del sistema, ma non cifrano il filesystem del tablet e non impediscono l’accesso a root. Le credenziali cloud sono separate dalle esportazioni dei quaderni e dalle copie locali della libreria.

Un lock privato non bloccante evita che processi Quaderno concorrenti sostituiscano le credenziali o l’autorizzazione temporanea dell’altro. Token e risposte di autorizzazione non entrano nei messaggi di stato, nei log o nei documenti. I metadati del browser scadono e vengono rimossi al termine, all’annullamento o al blocco PIN. I download sono file privati di appoggio: il flusso d’importazione elimina il proprio file tracciato dopo annullamento o elaborazione conclusa; le importazioni asincrone lo mantengono fino al termine. La pulizia rifiuta percorsi estranei e link simbolici e non elimina ricorsivamente directory.

La disconnessione locale elimina solo le credenziali su questo dispositivo. Dropbox e Google permettono anche una revoca esplicita presso il servizio. La revoca Google può interessare altri client e scope dello stesso progetto: per questo è una scelta separata nella conferma. Quaderno non chiama la revoca globale delle sessioni Microsoft; per revocare quell’app usa la pagina delle autorizzazioni del tuo account. [Revoca Dropbox](https://docs.dropboxapi.com/dropbox-api/api-reference/user-endpoints/auth/token-revoke), [revoca Google](https://developers.google.com/identity/protocols/oauth2/limited-input-device#tokenrevoke), [portata della revoca Microsoft](https://learn.microsoft.com/en-us/graph/api/user-revokesigninsessions?view=graph-rest-1.0).

## Collaudo e verifiche ancora necessarie

La suite `cloud_tests` usa rete simulata e archivi temporanei privati: non contatta i fornitori e non modifica il Portachiavi dell’utente. Verifica PKCE, stato del callback, polling, rinnovo dei token, cancellazione, callback scaduti, limiti, permessi, lock, nomi, conflitti di caricamento e redirect senza inoltro del bearer. `python3 tests/cloud_helper_tests.py` verifica offline metadati e chiusura delle connessioni SSH.

Prima di dichiarare pronto un collegamento di produzione, servono registrazione di proprietà del progetto e account reale: consenso iniziale, rinnovo dopo la scadenza, paginazione delle cartelle, download, caricamento di una nuova copia, annullamento e revoca/disconnessione. Google va provato sia con l’assistente SSH del tablet sia con il ritorno locale del Mac; il Portachiavi va verificato con l’app macOS effettivamente firmata. Questi collaudi live restano da eseguire quando saranno disponibili registrazioni e autenticazione dell’utente.
