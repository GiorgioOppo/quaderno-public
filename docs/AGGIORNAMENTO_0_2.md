# Quaderno 0.2 — note di sviluppo

L’aggiornamento sviluppa le dieci migliorie esaminate per Paper Pro e Mac. Aggiorna entrambe le app per scambiare documenti con i nuovi dati di recupero, rotazione, lingua e lettura da sorgente. I precedenti quaderni portabili restano leggibili.

## Lazo e scrittura

1. **Ripristino della scrittura originale.** La conversione conserva i tratti della penna e il loro livello nel blocco di testo. Puoi recuperarli dopo un cambio pagina, la riapertura dell’app o un trasferimento. Anche modificando la trascrizione, i dati originali rimangono disponibili. Annulla e Ripeti funzionano nella sessione di modifica corrente.
2. **Selezione sul foglio.** Puoi spostare, ridimensionare e ruotare una selezione con inchiostro, testo, immagini, tabelle e grafici. Trasformazione ed eliminazione sono modifiche uniche annullabili. Prima di applicarle, Quaderno controlla che pagina ed elementi corrispondano ancora alla selezione.
3. **Due regole e selezione incrementale.** La modalità Contenuti seleziona gli elementi interamente racchiusi. Toccati comprende quelli intersecati. Aggiungi e Sottrai permettono di correggere la selezione esistente.
4. **Trascrizione correggibile in posizione.** Ogni quaderno può usare italiano o inglese. L’anteprima mantiene le righe e permette correzioni prima della sostituzione. Le alternative compaiono quando il motore le restituisce. Sul Paper Pro il pacchetto opzionale WritePad deve includere entrambi i dizionari.

Consulta [Lazo](LASSO.md) per i comandi.

## Documenti, salvataggio e trasferimento

5. **Salvataggi per pagina e risorsa.** Un indice salvato atomicamente indica gli oggetti immutabili di pagine e risorse. Le parti invariate vengono riutilizzate. La versione precedente resta disponibile per il recupero e la migrazione conserva il JSON originale. Due istanze dell’app non possono sovrascriversi senza rilevare la modifica.
6. **Rendering della pagina richiesta.** L’importazione PDF/EPUB lavora in background con avanzamento e annullamento. I PDF conservano il sorgente; gli EPUB conservano l’archivio originale e un’impaginazione PDF vettoriale, uguale sui due dispositivi. Il lettore rasterizza la pagina richiesta alla risoluzione dello zoom, usa una cache limitata e scarta i risultati superati. Due dita ingrandiscono; un dito sposta la pagina ingrandita o cambia pagina quando è adattata allo schermo.
7. **Sync riprendibile.** Le nuove app scambiano gli indici e solo gli oggetti mancanti. Ogni blocco viene salvato prima della conferma. Dopo una disconnessione si riparte dall’ultimo punto disponibile, richiedendo una nuova anteprima e conferma prima di applicare il contenuto. Restano i controlli sui conflitti e sulle anteprime scadute. Vedi [Protocollo wireless](PROTOCOLLO_SYNC.md).

Per un backup locale conserva **l’intera cartella dati dell’app**, inclusi oggetti e indice precedente. Il solo indice locale non è un quaderno portabile. L’esportazione USB e il trasferimento wireless ricostruiscono il JSON con le risorse. L’esportazione USB dell’intera libreria ha un limite di 64 MiB; per librerie più grandi trasferisci un quaderno alla volta. Il rendering di una pagina alla volta riduce il lavoro del lettore, mentre le dimensioni del trasferimento dipendono comunque dai sorgenti conservati.

## Apple, standby e verifica

8. **Stati chiari per Apple.** Note usa un Markdown con risorse; Libri un PDF; Numbers la prima tabella della pagina in CSV con i valori; Keynote un PDF preparato; Diario il menu Condividi di macOS. Coda e file sono persistenti. L’apertura dell’app lascia l’esportazione in attesa. Completa l’importazione nella destinazione e scegli **Ho importato il file**, oppure Riapri o Annulla. Annulla conserva i file. Il CSV per Numbers contiene valori; il trasferimento fra app Quaderno conserva le formule modificabili.
9. **Penna e standby coordinati.** L’attività nativa del Marker aggiorna il timer. Lo standby viene rimandato durante scrittura, importazione, riconoscimento e finestre di modifica. La schermata di pausa blocca l’inchiostro anche senza PIN. Il risveglio mantiene il documento e richiede il PIN se configurato. AppLoad avvia una sessione persistente; all’uscita vengono ripristinati interfaccia originale e autosleep. La schermata di pausa dell’app non sospende il kernel.
10. **Test di regressione.** Le suite verificano geometria del lazo, recupero dopo riapertura, trasformazioni miste, formule, conferme scadute, recupero dei salvataggi, lettura da sorgenti, trasferimenti interrotti e dati danneggiati. Il controllo di avvio richiede un fotogramma e un ciclo eventi reattivo. La CI esegue i test desktop su macOS e Linux ARM e offre una compilazione manuale con l’SDK Paper Pro corrispondente.

Esegui `ctest --test-dir build --output-on-failure` sulla build desktop e `python3 scripts/check-startup.py /percorso/quaderno`. Per l’SDK tablet privo di Qt Test usa `-DBUILD_TESTING=OFF`. La prova del layout tablet sul Mac non verifica schermo fisico, Marker o Bluetooth: registra questi collaudi separatamente per la build installata.

Consulta il [registro delle verifiche 0.2](VERIFICA_0_2.md) per i collaudi completati, i tempi misurati sul Mac e le prove ancora da eseguire sul dispositivo fisico.
