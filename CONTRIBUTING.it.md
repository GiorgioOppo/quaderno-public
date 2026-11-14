# Contribuire a Quaderno

[English](CONTRIBUTING.md) · **Italiano**

Per una proposta o un problema riproducibile, apri la scheda **Issues** del repository. Per informazioni sensibili segui le istruzioni per le segnalazioni riservate. [Politica di sicurezza](SECURITY.it.md)

## Segnalare un problema

Indica la build o il commit, il sistema operativo e, per il Paper Pro, il modello e la versione completa del firmware. Descrivi i passaggi per riprodurre il problema, il comportamento atteso e quello osservato. Specifica se hai provato sul tablet fisico oppure in una simulazione.

Se serve un allegato, usa un quaderno di esempio con il minimo contenuto utile. Rimuovi credenziali, documenti personali, numeri di serie e dati di rete dagli screenshot e dai log.

## Proporre una modifica

1. Crea un branch da `main` e descrivi la modifica nella pull request.
2. Segui la [guida di sviluppo](docs/SVILUPPO.md) per la build. Per le modifiche QML controlla anche una finestra stretta e la modalità tablet. Per le modifiche alla libreria verifica che i file esistenti si aprano e che salvataggio e riapertura conservino i dati.
3. Riporta nella pull request i test eseguiti e le parti che richiedono ancora un collaudo. Aggiorna la guida d’uso o di installazione in entrambe le lingue quando cambia un flusso documentato.

Il progetto usa C++17 e Qt Quick. I sorgenti, gli script e la documentazione vanno nel repository. I binari di distribuzione vengono preparati come pacchetti di rilascio. [Preparazione del rilascio](docs/RILASCIO.md)

I contributi seguono la licenza dei file che modificano. Quaderno usa la [licenza MIT](LICENSE) e il componente opzionale WritePad usa la GPL-3.0-or-later. [Licenze dei componenti di terze parti](THIRD_PARTY_NOTICES.md)
