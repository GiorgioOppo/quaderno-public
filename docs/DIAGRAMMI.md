# Diagrammi

[English](en/DIAGRAMS.md) · **Italiano**

La sezione **Diagrammi** raccoglie i diagrammi di flusso modificabili accanto a Libri, Scrittura, Grafici e Tabelle. Un diagramma contiene forme, etichette e collegamenti diretti e può condividere la pagina con scrittura a mano, immagini, testo, grafici e tabelle.

Questa guida include l’estensione dei diagrammi esistenti nella **0.2.10 in sviluppo**. Le build Mac e ARM hanno superato tutte le sette suite automatiche interessate e gli avvii rigorosi desktop/tablet. Le nuove prove coprono anteprima completa, scelta della destinazione, etichette e posizioni conservate, inchiostro mantenuto, annulla e recupero dopo riapertura. Il 18 novembre 2026 l’utente ha confermato l’estensione sul Paper Pro: anteprima e risultato applicato contengono il diagramma completo aggiornato come unico elemento modificabile, con le etichette precedenti conservate. L’app Mac installata resta alla 0.2.0.

Le verifiche precedenti riguardano il riconoscimento della 0.2.8: la prima installazione ha superato controlli automatici/nativi e l’utente ha confermato la rotazione automatica da verticale a orizzontale. Un disegno a mano libera reale ha evidenziato un riconoscimento troppo restrittivo; le tolleranze riviste descritte sotto hanno superato verifiche geometriche/UI e avvii rigorosi Mac e ARM. La build corretta è stata installata sul Paper Pro e ha superato l’avvio rigoroso nativo/AppLoad a 1620 × 2160 senza errori QML. Il 18 novembre 2026 l’utente ha confermato che riconoscimento, applicazione del risultato e apertura dell’editor per modificarlo funzionano nel diagramma di prova sul Paper Pro. La successiva 0.2.9, già verificata e installata, riguarda gli aggiornamenti della sincronizzazione; queste conferme non costituiscono un collaudo dell’estensione 0.2.10. Vedi lo [stato delle verifiche](IMPOSTAZIONI.md#ambito-delle-verifiche) e il [registro modifiche](../CHANGELOG.it.md).

## Creazione e modifica

Apri **Diagrammi** per creare un quaderno dedicato oppure trovare i diagrammi già presenti in altri quaderni. Un quaderno di diagrammi vuoto rimane accessibile anche prima della prima forma riconosciuta. Aprire una voce della libreria mostra la pagina; l’azione di modifica apre l’editor.

Puoi disegnare con la penna sulla pagina oppure costruire il diagramma nell’editor. Sono disponibili processo, inizio/fine, decisione, dati e nota, con etichette e frecce modificabili. L’editor consente spostamento e ridimensionamento, selezione e lazo, duplicazione, griglia, allineamento, distribuzione, zoom, collegamenti diretti o ortogonali e annulla/ripristina della bozza.

Salvare nell’editor aggiorna l’oggetto diagramma. La cronologia della bozza nell’editor e Annulla sulla pagina sono meccanismi distinti.

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

Il riconoscimento del testo è distinto da quello delle forme e usa il motore di scrittura già installato con Quaderno. L’analisi geometrica funziona offline senza nuovi modelli AI o account cloud. Disponibilità e lingue dell’OCR dipendono dal programma di riconoscimento e dai dizionari installati.

I tratti interamente contenuti in una forma sono proposti come sua etichetta. Un piccolo gruppo vicino a un collegamento può essergli assegnato solo quando l’associazione è univoca. Contorni e punte delle frecce vengono esclusi dall’OCR.

Puoi modificare o escludere ciascuna etichetta. Se l’OCR fallisce, non è disponibile o viene interrotto, un’etichetta vuota non impedisce di applicare le forme: i tratti non trascritti rimangono sulla pagina. Anche quelli delle etichette escluse vengono conservati. Inserire manualmente un’etichetta accettata e non vuota permette di sostituire i tratti associati quando la sostituzione è attiva.

La finestra usa un riconoscitore indipendente. Chiuderla, cambiare pagina o livello oppure bloccare Quaderno interrompe il lavoro. I risultati tardivi non possono modificare un’altra pagina. Se il documento cambia dopo la preparazione dell’anteprima, l’applicazione viene rifiutata e serve una nuova selezione.

## Applicazione, annullamento e recupero

La conversione verifica selezione e diagramma e applica insieme la creazione o l’aggiornamento dell’oggetto e l’eventuale rimozione dei tratti. **Annulla sulla pagina corrente** ripristina l’intera conversione, compresi il diagramma precedente e i nuovi tratti nel caso di un’estensione; Ripristina la applica di nuovo. Questa cronologia appartiene alla sessione della pagina e del livello, quindi non equivale a una cronologia permanente dopo navigazione o riavvio.

Con la sostituzione attiva, i tratti originali rimossi vengono conservati anche come origine manoscritta del diagramma. L’azione separata per ripristinare la scrittura originale permette di recuperarli dopo il salvataggio e la riapertura del quaderno. Questo recupero è distinto dall’Annulla della sessione. Per un diagramma ampliato, conserva una base modificabile precedente alle aggiunte e raccoglie i tratti rimossi dalle estensioni successive sullo stesso livello. Il recupero ripristina quella base e restituisce i tratti delle aggiunte: non offre una cronologia permanente di ogni passaggio. Per annullare soltanto l’ultima estensione nella sessione corrente, usa Annulla sulla pagina. Se esistono aggiunte recuperabili su un altro livello, torna a quel livello prima di riconoscere altre aggiunte.

Con **Sostituisci i tratti convertiti con forme e testo** disattivato, la scrittura rimane direttamente sulla pagina e non viene allegata una nuova copia per il recupero; un’origine già presente resta conservata. Applicare un diagramma nasconde il suggerimento automatico corrente anche conservando l’inchiostro, così lo stesso schizzo invariato non viene subito proposto di nuovo. Una successiva modifica dei tratti o il cambio di pagina/livello può consentire una nuova proposta, purché siano riconosciute aggiunte valide; il riconoscimento manuale resta disponibile.

I tratti non riconosciuti e i gruppi di testo esclusi o vuoti non vengono rimossi dalla conversione.

## Utilizzare il diagramma nelle altre sezioni

- **Scrittura:** copia il diagramma in un quaderno di scrittura come oggetto modificabile. L’esportazione Markdown con allegati include un’immagine e un file `.drawio`; il solo testo Markdown non può contenere la geometria modificabile di Quaderno.
- **Libri, Grafici e Tabelle:** diagrammi e altri oggetti possono convivere sulla pagina. Copiare un oggetto crea una copia indipendente, non un collegamento che aggiorna automaticamente entrambi i documenti.
- **PDF:** l’esportazione della pagina disegna il diagramma insieme agli altri contenuti.
- **draw.io:** importa diagrammi `.drawio`/XML supportati ed esporta file `.drawio` modificabili. Elementi esterni non supportati possono essere omessi o semplificati: controlla gli avvisi dell’importazione.
- **Trasferimento e backup:** il documento portabile `.quaderno` e i flussi di trasferimento e backup dei quaderni conservano il modello modificabile. Non serve un nuovo servizio dedicato ai diagrammi.

Aggiorna **entrambe le app alla 0.2.10 o successiva** prima di trasferire diagrammi ampliati che richiedono il **formato portabile 10**. Questo formato conserva la scala visiva del diagramma e la base modificabile per il recupero delle aggiunte. Il formato minimo viene scelto in base ai contenuti: un quaderno senza questi nuovi metadati può mantenere una versione precedente. L’app Mac 0.2.0 installata non è stata aggiornata e non può importare il formato 10; anche la 0.2.9 precede questo supporto. Vedi [versioni e recupero dei dati](DATI.md#versioni-della-libreria).

Le versioni precedenti alla 0.2.8 possono inoltre omettere la classificazione del quaderno Diagrammi o i metadati della scrittura originale del diagramma durante importazione ed esportazione. Un’app nuova conserva la classificazione locale esistente quando un aggiornamento da un’app precedente la omette, ma questo non rende completamente privo di perdite un passaggio di andata e ritorno attraverso una vecchia app. Conserva un backup del quaderno dalla versione attuale prima di usare un dispositivo con versione precedente. Copiare un diagramma in un altro quaderno conserva volutamente geometria ed etichette modificabili senza la sorgente di recupero dei tratti della pagina originale.

## Limiti e suggerimenti per il disegno

Il modello supporta **64 nodi** e **128 collegamenti diretti** per diagramma, con **200 caratteri per etichetta di nodo** e **120 per etichetta di collegamento**. Il riconoscimento accetta al massimo **4.000 tratti** e **120.000 punti** e rifiuta disegni con oltre **256 segmenti aperti candidati**. Il superamento dei limiti interrompe la conversione senza applicarne una parte.

Le forme vengono riconosciute con criteri prudenti. Nelle coordinate logiche della pagina 600 × 800, il rettangolo d’ingombro deve misurare almeno 30 × 24 unità e avere area di almeno 1.200 unità quadrate. Forme grandi e ben chiuse funzionano meglio di segni piccoli quanto una lettera. Queste coordinate non dipendono dai pixel fisici del display. Schizzi complessi, curve arbitrarie, rettangoli ruotati, collegamenti senza nodi, gesti singoli senza una punta triangolare chiaramente riconosciuta e percorsi personalizzati esatti restano fuori dal riconoscimento supportato. Il gesto unico asta-triangolo supportato non implica il riconoscimento di qualsiasi freccia continua.

Usa l’editor strutturato per correggere o ampliare il risultato. Il riconoscimento automatico propone un diagramma modificabile, senza garantire l’interpretazione corretta di ogni disegno.
