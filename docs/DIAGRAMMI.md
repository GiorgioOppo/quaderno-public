# Diagrammi

[English](en/DIAGRAMS.md) · **Italiano**

La sezione **Diagrammi** raccoglie diagrammi di flusso e UML modificabili accanto a Libri, Scrittura, Grafici e Tabelle. Un diagramma contiene forme, etichette e collegamenti diretti e può condividere la pagina con scrittura a mano, immagini, testo, grafici e tabelle.

L’**anteprima dei sorgenti 0.2.23** aggiunge nove forme UML all’editor dei diagrammi. [Cronologia delle modifiche](../CHANGELOG.it.md)

## Creazione e modifica

Apri **Diagrammi** per creare un quaderno dedicato oppure trovare i diagrammi già presenti in altri quaderni. Un quaderno di diagrammi vuoto rimane accessibile anche prima della prima forma riconosciuta. Aprire una voce della libreria mostra la pagina; l’azione di modifica apre l’editor.

Apri **Forme e connettori** per costruire il diagramma. Su uno schermo ampio la palette e l’ispettore sono ai lati del foglio. Quando lo spazio è ridotto, sono sopra e sotto l’area di disegno.

1. Tocca o trascina dalla palette un processo, un inizio/fine, una decisione, una forma per i dati o una nota. Apri **Altre forme** per database, sottoprocesso, display, input manuale, preparazione, attesa, connettore fuori pagina e nuvola. Trascina la forma sul foglio per spostarla e usa la maniglia di ridimensionamento.
2. Trascina la maniglia del connettore dalla prima forma alla seconda, oppure scegli **Collega** e tocca in ordine le due forme.
3. Seleziona una forma o una freccia per modificarne etichetta e proprietà nell’ispettore. Puoi cambiare il tipo di forma, duplicarla, invertire il verso della freccia o scegliere un percorso diretto od ortogonale.
4. Usa il lazo e i comandi di disposizione per allineare e distribuire le forme. Griglia, zoom e panoramica aiutano a posizionarle. Usa Annulla e Ripristina nell’editor durante la modifica della bozza.

Per lavorare a mano libera, disegna sul foglio e usa **Riconosci diagramma**. Dalla stessa barra puoi importare un file draw.io compatibile.

Salvare nell’editor aggiorna l’oggetto diagramma. La cronologia della bozza nell’editor e Annulla sulla pagina sono meccanismi distinti.

## Creare un diagramma UML

In **Forme e connettori**, apri **UML**. Tocca o trascina **Classe**, **Interfaccia**, **Oggetto**, **Attore**, **Caso d’uso**, **Componente**, **Pacchetto**, **Nodo** (di distribuzione) o **Linea di vita**. Seleziona una forma per modificarne l’etichetta e usa i connettori dell’editor per collegarla alle altre.

In **Modifica etichetta**, separa il nome e gli attributi con una riga vuota. Per una classe o un’interfaccia, separa allo stesso modo le operazioni dagli attributi. Con la tastiera sullo schermo, usa **A capo** due volte per inserire una riga vuota.

## Scambiare diagrammi con draw.io

Importa un file `.drawio` o XML compatibile per modificare le forme, le etichette e i collegamenti diretti supportati nella prima pagina. La palette estesa associa i cilindri supportati ai database, i processi predefiniti ai sottoprocessi e gli esagoni alla preparazione. Il simbolo di input manuale ora ha una propria forma. I parallelogrammi rappresentano i dati e gli ovali l’inizio o la fine. I quaderni creati con la palette originale conservano le forme salvate. Controlla gli avvisi d’importazione per gli elementi omessi e gli stili semplificati.

Lo scambio `.drawio` include i nove simboli UML della palette. Per identificare un caso d’uso all’importazione, usa uno stile UML esplicito nel file. Quaderno interpreta un’ellisse generica come forma di inizio o fine. Le decorazioni delle relazioni UML possono essere semplificate durante l’importazione.

Esporta in `.drawio` dal menu del diagramma per continuare a modificare forme e collegamenti supportati in draw.io. Per conservare pagine, annotazioni e collegamenti interni, trasferisci il file nativo `.quaderno`.

## Riconoscere un diagramma disegnato a mano

1. Disegna forme chiuse sufficientemente distanziate e collegale con linee e punte di freccia visibili. Scrivi le etichette all’interno delle forme.
2. In un quaderno Diagrammi, dopo una pausa nel disegno l’analisi automatica può proporre un suggerimento per la pagina corrente. Non apre finestre e non modifica la pagina da sola. **Chiudi** lo nasconde fino a una modifica dei tratti o al cambio di pagina/livello; anche disattivare e riattivare la preferenza automatica consente una nuova proposta. Puoi sempre selezionare il disegno con il lazo e richiedere esplicitamente il riconoscimento.
3. Apri il suggerimento o l’azione del lazo per rivedere l’anteprima. Durante la revisione il documento originale rimane invariato.
4. Controlla le etichette delle forme e dei collegamenti. Il motore locale disponibile riconosce un gruppo alla volta; puoi correggere il testo con tastiera a schermo o esterna, riprovare una trascrizione oppure interrompere il riconoscimento e scrivere manualmente.
5. Scegli se sostituire i tratti convertiti o conservare tutti gli originali, quindi premi **Applica diagramma**.

Il rettangolo diventa un processo, il rombo una decisione e l’ovale una forma di inizio/fine. Quest’ultima viene disegnata come capsula arrotondata, senza mantenere un’ellisse esatta. Posizioni e dimensioni relative dei nodi vengono conservate; riordino e allineamento restano azioni esplicite.

Il contorno può essere un unico tratto oppure da due a quattro segmenti con estremi vicini e univoci. Sono tollerate interruzioni limitate. Gli ovali possono essere ripassati nella chiusura o terminare con una piccola coda; un rombo leggermente inclinato disegnato in due tratti può diventare una decisione. Le tolleranze non trasformano anelli arbitrari o forme personalizzate in nodi.

I collegamenti possono essere rettilinei, seguire un semplice percorso ortogonale oppure avere una lieve curva semplificabile in un’asta retta. Le punte supportate comprendono V separata, due alette separate o triangolo chiuso/ripassato. È supportato anche un gesto continuo che disegna l’asta e poi una punta triangolare riconoscibile; la punta deve stabilire un verso chiaro vicino a una destinazione valida. Curve ampie, anelli ambigui e gesti singoli non supportati restano inchiostro. Due punte chiare alle estremità producono due collegamenti diretti. L’incrocio di due linee non crea un nodo.

Una linea senza punta chiara, una destinazione ambigua, una forma non supportata o un tratto estraneo rimangono scrittura a mano. Il verso non viene dedotto dall’ordine di disegno. Non vengono aggiunti auto-collegamenti o duplicati con lo stesso verso.

La versione 0.2.11 riconosce anche ovali irregolari e forme di processo a quattro lati con una piccola apertura nel contorno. Un singolo tratto può includere un gomito e una punta esplicita. Una punta separata può iniziare con un breve proseguimento della linea. Controlla forme e collegamenti prima di applicare. Le linee senza una direzione chiara restano a penna.

## Ampliare un diagramma già convertito

Puoi aggiungere nuove forme a penna accanto al diagramma oppure disegnare soltanto una freccia tra due nodi esistenti. Riconosci di nuovo i tratti: l’anteprima include **l’intero diagramma aggiornato**, con le forme e i collegamenti precedenti insieme alle nuove aggiunte. Il riepilogo distingue il numero di forme e collegamenti nuovi.

Se sulla pagina c’è un solo diagramma, viene usato come destinazione. Anche una selezione che contiene un solo diagramma può identificarlo. Quando sono possibili più destinazioni, scegli nella finestra il diagramma da aggiornare; **Applica diagramma** rimane disabilitato finché la scelta non è risolta. Puoi scegliere di creare un diagramma separato quando il disegno è indipendente. Gli altri diagrammi sulla pagina non vengono modificati.

L’applicazione aggiorna lo stesso oggetto, conservando gli identificatori, le etichette, i percorsi dei collegamenti e la posizione e dimensione sulla pagina delle parti esistenti. Se il riquadro complessivo deve allargarsi, la dimensione visiva di testo e linee già presenti viene conservata. L’analisi tiene conto anche della rotazione dell’oggetto esistente; questo non estende il riconoscimento a qualsiasi forma ruotata disegnata a mano.

Le etichette già presenti rimangono in sola lettura durante questa revisione e non vengono riscritte dall’OCR. Per cambiarle, applica le aggiunte e apri l’editor del diagramma. Puoi invece correggere le etichette delle nuove parti prima di applicare. Se non vengono trovate nuove forme o nuovi collegamenti validi, non è possibile applicare una conversione vuota. I contorni o collegamenti già riconosciuti non vengono duplicati quando hai conservato l’inchiostro originale; i tratti incerti restano sulla pagina.

## Etichette e conservazione della scrittura

**Riconosci diagramma** raggruppa i tratti originali del Marker per il motore di scrittura locale. Scrivi ogni etichetta all’interno del contorno, lasciando spazio tra il testo e il bordo. Geometria e scrittura vengono elaborate sul dispositivo. Le lingue disponibili dipendono dal motore e dai dizionari installati.

Quaderno usa i contorni riconosciuti per associare i tratti interni alle forme. Può proporre anche un piccolo gruppo vicino a un collegamento quando l’associazione è univoca. Controlla la destinazione evidenziata prima di applicare.

Dalla 0.2.21 Quaderno ingrandisce le scritte piccole del Marker per riconoscerle e prova a orientare le righe ruotate in base all’ordine dei tratti. Se una proposta è errata, seleziona l’etichetta, scegli **Orizzontale**, **Lettere in colonna**, **Ruota a destra** o **Ruota a sinistra** e premi **Riconosci di nuovo questa etichetta**. L’operazione usa una copia temporanea dei tratti.

Su una **pagina mista** in PDF o con sfondo immagine sul Paper Pro, l’OCR opzionale propone le etichette delle forme rilevate nell’area selezionata. Il motore prova i ritagli verticali nelle due direzioni e cerca lettere dritte disposte in colonna. Dalla 0.2.23 cerca anche le scritte minute appena sopra o sotto i riquadri e prova più letture su ritagli orizzontali piccoli. I tratti incompleti, le forme sovrapposte e le etichette tra due nodi possono richiedere una correzione manuale. Apri **Rivedi forme ed etichette** per confrontare la bozza con la pagina originale e correggere il testo di ogni forma prima di salvare. [Riconoscimento di una pagina mista](GUIDA.md#riconoscere-una-pagina-mista-sul-paper-pro) · [Motore OCR](../tools/raster-ocr/README.md)

Puoi modificare o escludere ciascuna etichetta. Se l’OCR fallisce, non è disponibile o viene interrotto, un’etichetta vuota non impedisce di applicare le forme: i tratti non trascritti rimangono sulla pagina. Anche quelli delle etichette escluse vengono conservati. Inserire manualmente un’etichetta accettata e non vuota permette di sostituire i tratti associati quando la sostituzione è attiva.

La finestra usa un riconoscitore indipendente. Chiuderla, cambiare pagina o livello oppure bloccare Quaderno interrompe il lavoro. I risultati tardivi non possono modificare un’altra pagina. Se il documento cambia dopo la preparazione dell’anteprima, l’applicazione viene rifiutata e serve una nuova selezione.

## Applicazione, annullamento e recupero

La conversione verifica selezione e diagramma e applica insieme la creazione o l’aggiornamento dell’oggetto e l’eventuale rimozione dei tratti. **Annulla sulla pagina corrente** ripristina l’intera conversione, compresi il diagramma precedente e i nuovi tratti nel caso di un’estensione; Ripristina la applica di nuovo. Questa cronologia appartiene alla sessione della pagina e del livello, quindi non equivale a una cronologia permanente dopo navigazione o riavvio.

Con la sostituzione attiva, i tratti originali rimossi vengono conservati anche come origine manoscritta del diagramma. L’azione separata per ripristinare la scrittura originale permette di recuperarli dopo il salvataggio e la riapertura del quaderno. Questo recupero è distinto dall’Annulla della sessione. Per un diagramma ampliato, conserva una base modificabile precedente alle aggiunte e raccoglie i tratti rimossi dalle estensioni successive sullo stesso livello. Il recupero ripristina quella base e restituisce i tratti delle aggiunte: non offre una cronologia permanente di ogni passaggio. Per annullare soltanto l’ultima estensione nella sessione corrente, usa Annulla sulla pagina. Se esistono aggiunte recuperabili su un altro livello, torna a quel livello prima di riconoscere altre aggiunte.

Con **Sostituisci i tratti convertiti con forme e testo** disattivato, la scrittura rimane direttamente sulla pagina e non viene allegata una nuova copia per il recupero; un’origine già presente resta conservata. Applicare un diagramma nasconde il suggerimento automatico corrente anche conservando l’inchiostro, così lo stesso schizzo invariato non viene subito proposto di nuovo. Una successiva modifica dei tratti o il cambio di pagina/livello può consentire una nuova proposta, purché siano riconosciute aggiunte valide; il riconoscimento manuale resta disponibile.

I tratti non riconosciuti e i gruppi di testo esclusi o vuoti non vengono rimossi dalla conversione.

## Utilizzare il diagramma nelle altre sezioni

- **Scrittura:** copia il diagramma in un quaderno di scrittura come oggetto modificabile. L’esportazione Markdown con allegati include un’immagine e un file `.drawio`; il solo testo Markdown non può contenere la geometria modificabile di Quaderno.
- **Libri:** usa **Collega elemento** per aprire il diagramma originale da una pagina del libro e tornare al punto di partenza. Trasferisci a parte il quaderno del diagramma quando si trova in un quaderno diverso.
- **Grafici e Tabelle:** diagrammi e altri oggetti possono convivere sulla pagina. Copiare un oggetto crea una copia indipendente, non un collegamento che aggiorna automaticamente entrambi i documenti.
- **PDF:** l’esportazione della pagina disegna il diagramma insieme agli altri contenuti.
- **draw.io:** importa diagrammi `.drawio`/XML supportati ed esporta file `.drawio` modificabili. Elementi esterni non supportati possono essere omessi o semplificati: controlla gli avvisi dell’importazione.
- **Trasferimento e backup:** il documento portabile `.quaderno` e i flussi di trasferimento e backup dei quaderni conservano il modello modificabile. Non serve un nuovo servizio dedicato ai diagrammi.

Usa **0.2.23 o successiva su entrambi i dispositivi** per trasferire un quaderno con forme UML, che richiedono il **formato portabile 14**. Le forme di flusso aggiunte nella 0.2.20 richiedono il **formato 13**. Quaderno sceglie la versione minima richiesta dai contenuti, comprese le forme presenti nella base di recupero. [Versioni e recupero dei dati](DATI.md#versioni-della-libreria)

Le versioni precedenti alla 0.2.8 possono inoltre omettere la classificazione del quaderno Diagrammi o i metadati della scrittura originale del diagramma durante importazione ed esportazione. Un’app nuova conserva la classificazione locale esistente quando un aggiornamento da un’app precedente la omette, ma questo non rende completamente privo di perdite un passaggio di andata e ritorno attraverso una vecchia app. Conserva un backup del quaderno dalla versione attuale prima di usare un dispositivo con versione precedente. Copiare un diagramma in un altro quaderno conserva volutamente geometria ed etichette modificabili senza la sorgente di recupero dei tratti della pagina originale.

## Limiti e suggerimenti per il disegno

Il modello supporta **64 nodi** e **128 collegamenti diretti** per diagramma, con **200 caratteri per etichetta di nodo** e **120 per etichetta di collegamento**. Il riconoscimento accetta al massimo **4.000 tratti** e **120.000 punti** e rifiuta disegni con oltre **256 segmenti aperti candidati**. Il superamento dei limiti interrompe la conversione senza applicarne una parte.

Le forme vengono riconosciute con criteri prudenti. Nelle coordinate logiche della pagina 600 × 800, il rettangolo d’ingombro deve misurare almeno 30 × 24 unità e avere area di almeno 1.200 unità quadrate. Forme grandi e ben chiuse funzionano meglio di segni piccoli quanto una lettera. Queste coordinate non dipendono dai pixel fisici del display. Schizzi complessi, curve arbitrarie, rettangoli ruotati, collegamenti senza nodi, gesti singoli senza una punta triangolare chiaramente riconosciuta e percorsi personalizzati esatti restano fuori dal riconoscimento supportato. Il gesto unico asta-triangolo supportato non implica il riconoscimento di qualsiasi freccia continua.

Usa l’editor strutturato per correggere o ampliare il risultato. Il riconoscimento automatico propone un diagramma modificabile, senza garantire l’interpretazione corretta di ogni disegno.
