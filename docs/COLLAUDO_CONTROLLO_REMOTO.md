# Collaudo del controllo remoto di Quaderno

Esegui prima i test in un profilo isolato, poi collega Mac e Paper Pro sulla stessa rete Wi-Fi. Usa documenti sintetici e copie dei quaderni esistenti. Conserva una copia della libreria prima dell’aggiornamento. Per la prova fisica, apri **Schermo del Paper Pro** sul Mac e spunta **Consenti il controllo dal Mac** sul tablet. Esegui i clic sull’immagine in diretta e digita dalla tastiera del Mac dopo aver selezionato il campo nel fotogramma.

| Area | Prova | Esito atteso |
| --- | --- | --- |
| Consenso | Avvia in sola visualizzazione, concedi il controllo, revocalo e riavvia la condivisione. Prova lo stesso clic in ogni fase. | I comandi agiscono durante il consenso. La revoca lascia attivo il video. La nuova sessione richiede il consenso sul tablet. |
| Libreria | Passa a Libri, Scrittura, Grafici, Tabelle e Diagrammi. Apri una voce nella sezione attiva e torna indietro con Libreria. | Le intestazioni e il contenuto del fotogramma corrispondono alla sezione o alla pagina sul tablet. |
| Impostazioni | Apri Impostazioni e visita Generali, Schermo, Penna e batteria e Informazioni. Cerca «schermo» e torna alla libreria. | Le categorie cambiano. Confronta le preferenze prima e dopo la visita. |
| Libri | Prepara un PDF di due pagine di prova. Aprilo, passa alla pagina 2 e torna alla pagina 1. Aggiungi una nota, riaprila, modifica il testo e apri nuovamente il libro. | La posizione della nota resta associata alla pagina. Il testo aggiornato sopravvive alla riapertura. |
| Scrittura | Crea **QA Scrittura remota**, scrivi un’intestazione e una riga, salva e riapri. Sostituisci la riga, salva e riapri. | Titolo e Markdown corrispondono al testo salvato. |
| Grafici | Crea **QA Grafico remoto** con titolo e punti. Riapri il grafico, modifica titolo o valore e salva. | La pagina mostra il nuovo titolo o valore. I dati restano modificabili. |
| Tabelle | Crea **QA Tabella remota**, inserisci due numeri e una formula `=SUM(A1:A2)`. Riapri la tabella e cambia `A1`. | I valori e la formula si conservano. La somma si aggiorna con il nuovo valore. |
| Diagrammi | Crea **QA Diagramma remoto**, aggiungi una forma e assegna un’etichetta. Riaprilo e cambia l’etichetta. | Il diagramma resta un elemento modificabile e conserva l’etichetta salvata. |
| Geometria e focus | Ridimensiona la finestra e prova un clic sulle bande laterali. Digitando, passa da un campo a un altro e poi torna in libreria. | Il clic colpisce il controllo visibile. Le bande restano inattive e il testo entra solo nel campo scelto. |
| Interruzioni | Apri un pannello locale riservato, blocca il tablet o interrompi il Wi-Fi in una sessione di prova. | Il pannello sospende i comandi. Blocco o disconnessione terminano la diretta e svuotano l’anteprima. |

Per ogni prova registra il dispositivo, il comando eseguito, il risultato osservato e la persistenza alla riapertura. Indica se il risultato proviene dal profilo isolato o dai due dispositivi collegati. Conserva i documenti di prova per poter ripetere il collaudo.

## Esiti del 22 novembre 2026

Il Mac ha controllato Quaderno sul Paper Pro via Wi‑Fi. I clic sono stati eseguiti sull’immagine condivisa e il testo è stato inserito dalla tastiera del Mac. Sono stati usati cinque documenti sintetici già caricati per la prova e quattro creati durante il collaudo. Le modifiche e le annotazioni sono state salvate nei documenti di prova.

| Area | Verifica su Mac e Paper Pro | Risultato |
| --- | --- | --- |
| Libreria | Ho visitato tutte e cinque le sezioni, aperto i documenti di prova e usato **Libreria** per tornare all’elenco. | Sezioni, file e anteprime si sono aggiornati nella finestra del Mac. |
| Impostazioni | Ho aperto **Generali**, cercato `schermo` e visitato **Schermo** e **Penna e batteria**. | La ricerca ha filtrato le categorie. Le due schermate hanno mostrato i rispettivi controlli e lo stato della penna. |
| Libri | Ho aperto **QA Lettura remota A** e **QA Lettura remota B** e raggiunto pagina 2 in entrambi. Su A ho creato un post-it, riaperto il marcatore, modificato il testo e riaperto la nota. | La nota è rimasta ancorata alla seconda pagina e ha conservato il testo modificato. |
| Scrittura | Ho creato **QA Scrittura remota** con titolo, intestazione e testo su più righe. In **QA Appunti esistenti** ho sostituito e salvato la riga di testo. | Il nuovo Markdown è comparso in libreria. Il foglio preesistente ha mostrato la riga aggiornata dopo il salvataggio. |
| Grafici | Ho creato **QA Grafico remoto** con due punti e sostituito la prima coordinata X con `32`. Ho aperto l’editor del grafico preesistente con un clic sulla serie e ho salvato il nuovo titolo. | Il nuovo grafico ha mostrato la serie aggiornata. Il grafico preesistente ha conservato la serie e mostrato il nuovo titolo anche in libreria. |
| Tabelle | Ho salvato **QA Tabella remota** con `6`, `10` e `=AVERAGE(A1:A2)`. Nella tabella esistente ho cambiato `A1` da `4` a `9`, salvato e riaperto `B1`. | La media ha dato `8`. La somma è passata da `11` a `16` e `=SUM(A1:A2)` è rimasta nel campo modificabile di `B1`. |
| Diagrammi | Ho creato **QA Diagramma remoto**, aggiunto una forma Processo, salvato un’etichetta e riaperto il diagramma con un clic sul nodo. | Ho modificato l’etichetta in `Nodo QA aggiornato` e l’ho ritrovata nel foglio e in libreria. |
| Fine della diretta | Ho premuto **Interrompi** nella finestra del Mac e ho riaperto il ricevitore. | Il ricevitore ha chiesto di avviare una nuova condivisione dal Paper Pro. |

I test automatici ARM hanno verificato l’avvio esplicito e la revoca del controllo, l’aggiornamento del fotogramma, la navigazione con SMTP riservato al tablet, la creazione di Markdown, il salvataggio di un post-it, la modifica con formula in una tabella e la creazione e modifica di un grafico. I test isolati del visualizzatore su macOS hanno verificato la geometria dei clic, il focus della tastiera e i pannelli riservati.

Durante la prova su hardware, alcuni clic subito dopo un cambio schermata hanno richiesto una ripetizione sul fotogramma aggiornato. Con alcuni editor aperti la barra può mostrare due pulsanti **Interrompi condivisione** sovrapposti. Il comando sulla barra del tablet non ha interrotto la diretta quando premuto nel fotogramma dal Mac. L’arresto verificato usa **Interrompi** nella finestra Mac.

Restano da provare su hardware **Informazioni**, il ridimensionamento della finestra durante la diretta, il blocco con PIN, la caduta del Wi‑Fi e la revoca mantenendo il solo video. Il collaudo dei libri ha usato PDF.

### Prova di regressione per refresh e arresto della diretta

I test isolati Mac e ARM verificano il profilo per contenuti durante la condivisione, il ripristino del profilo selezionato alla fine della sessione e il clic remoto su **Interrompi condivisione** dalla barra e da un editor. Il ricevitore continua ad aggiornarsi quando cambia il contenuto e svuota l’immagine quando Quaderno viene bloccato.

Sul Paper Pro alterna due pagine sintetiche, una ad alto contrasto e una bianca, e apri e chiudi lo stesso editor. Ripeti la sequenza con diretta disattivata e attivata. Osserva sul pannello se rimangono testo, linee o bordi della schermata precedente. Controlla i tratti con il Marker e lascia la diretta inattiva per 15 secondi per verificare la presenza di lampeggi. Registra separatamente il risultato visibile sul pannello e la continuità della finestra sul Mac.

### Proporzioni e orientamento della finestra Mac

Il test isolato verifica una sorgente 3:4, il passaggio in orizzontale con anteprima 4:3, i limiti dell’area disponibile del monitor, la conservazione di una dimensione scelta a mano al fotogramma successivo e le coordinate di un clic dopo la rotazione. Le prove di collegamento verificano anche i fotogrammi delle app precedenti, privi dell’orientamento. Ripeti la rotazione via Wi-Fi con una build compatibile su entrambi i dispositivi e verifica che il clic agisca sul comando visibile nell’immagine.
