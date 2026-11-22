# Note collegate a un punto del libro

[English](en/BOOK_NOTES.md) · **Italiano**

Quaderno 0.2.11 in sviluppo aggiunge indicatori numerati di nota in un punto specifico della pagina. Una nota può contenere testo da tastiera, scrittura a mano oppure entrambi. La nota appartiene a quella pagina salvata in Quaderno: segue zoom e rotazione senza essere ricollegata a un altro paragrafo.

## Aggiungere, leggere e modificare

In **Libri**, scegli **Post-it**, poi tocca il punto della pagina in cui vuoi inserire l'indicatore. Scrivi nel campo di testo della nota, disegna nell'area per la scrittura a mano oppure combina i due contenuti. Scegli il colore e salva. Annullare una nuova bozza lascia la pagina invariata.

Tocca l'indicatore numerato per riaprire il contenuto. Modificare il testo o i tratti mantiene l'indicatore nella sua posizione. Usa **Elementi** sulla pagina per spostarlo, oppure l'editor della nota per eliminarlo. Annulla/Ripeti della pagina comprende creazione, modifica, spostamento ed eliminazione delle note nella sessione di modifica corrente. Prima di modificare le note occorre mostrare il livello d'inchiostro attivo se è nascosto.

Gli indicatori sono numerati nell'ordine delle note della pagina. Il numero può cambiare quando elimini una nota; l'identificativo persistente rimane lo stesso. Il testo della nota compare nella ricerca delle pagine, e le pagine contenenti note rientrano fra quelle annotate. La scrittura a mano rimane composta dai tratti originali modificabili: non viene trasformata di nascosto in testo da tastiera.

L'editor utilizza un'area di scrittura con proporzioni fisse 3:2. Ogni nota può contenere al massimo 4.000 caratteri e 2.000 tratti / 60.000 punti, entro il limite esistente di 120 elementi per pagina. Cambiare pagina con una bozza aperta, oppure modificare una copia superata della stessa nota, non può sovrascrivere silenziosamente un'altra pagina o un contenuto più recente.

## Trasferire fra Paper Pro e Mac

Invia il **quaderno di Quaderno**, non soltanto l'EPUB/PDF originale, per conservare evidenziazioni, sottolineature, livelli d'inchiostro e contenuti delle note. Il file originale del libro da solo non contiene le annotazioni di Quaderno. Importare separatamente lo stesso file crea un libro distinto: sincronizza lo stesso elemento della libreria di Quaderno sui due dispositivi.

Le note utilizzano il formato portabile della libreria **11**. Aggiorna entrambe le app alla 0.2.11 o successiva prima di trasferire un quaderno che contiene note. I contenuti che non richiedono il nuovo formato continuano a essere esportati con la versione minima necessaria. Il lettore dei dati e la verifica di compatibilità wireless condividono la versione massima supportata. Consulta [compatibilità dei dati](DATI.md) e [sincronizzazione wireless](PROTOCOLLO_SYNC.md).

## Esportare PDF e Markdown

L'esportazione PDF mantiene prima le pagine originali selezionate, con gli indicatori numerati. Aggiunge poi le note nell'ordine delle pagine di origine. Il testo viene impaginato senza tagliare le note lunghe; la scrittura a mano ha una pagina separata che conserva le proporzioni originali 3:2. Le intestazioni indicano pagina originale e numero della nota. Esportando soltanto la pagina corrente vengono aggiunte soltanto le note di quella pagina.

Il Markdown semplice include il testo delle note sotto un'intestazione con pagina e numero. **Markdown con allegati** include anche un PNG della scrittura a mano di ciascuna nota. Queste esportazioni sono copie leggibili: usa il formato del quaderno Quaderno per trasferire tratti e indicatori ancora modificabili.

## Stato delle verifiche

Le verifiche delle note sono passate su Mac e ARM. Coprono riapertura, protezione delle bozze, annulla e ripeti, appendici PDF complete e allegati Markdown. Le prove di interfaccia usano mouse, tocco ed eventi penna in verticale e orizzontale. I trasferimenti di libri sintetici conservano evidenziazioni, sottolineature, testo e scrittura modificabili. I percorsi fisici con penna e collegamento radio richiedono prove separate.
