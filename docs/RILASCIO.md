# Preparazione del rilascio

[English](en/RELEASE.md) · **Italiano**

## Anteprima dei sorgenti 0.2.19

Compila la 0.2.19 dal repository. Il tag pubblico `v0.1.0` identifica l’anteprima originale. Il repository include Quaderno, l’installer e le guide in inglese e italiano. Quaderno usa la licenza MIT e i motori di riconoscimento opzionali seguono le proprie licenze. [Tag originale](https://github.com/GiorgioOppo/quaderno-public/tree/v0.1.0) · [Licenza](../LICENSE) · [Componenti di terze parti](../THIRD_PARTY_NOTICES.md) · [Cronologia](../CHANGELOG.it.md)

Prima di rendere pubblico il repository, rivedi i file e i metadati dei commit, oltre alla visibilità delle issue, dei log delle verifiche automatiche e degli allegati. Usa dati di esempio nelle schermate e nei log. Esegui compilazione e prova di avvio sul commit definitivo e usa quel commit per il tag dell’anteprima. [Guida di sviluppo](SVILUPPO.md) · [Visibilità dei repository GitHub](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility)

## Verifiche

| Piattaforma o flusso | Verifiche eseguite | Verifiche per un pacchetto scaricabile |
| --- | --- | --- |
| Mac Apple Silicon | Compilazione e avvio. La trascrizione locale è stata verificata con il componente per macOS. | Prova il pacchetto firmato su un Mac pulito. Verifica la versione minima di macOS, l’importazione dei file e il recupero della libreria. |
| reMarkable Paper Pro (`ferrari`), firmware 3.29.1.0 | Suite ARM64 e prove di avvio con rendering. Verificati su un tablet fisico l’avvio a schermo intero da AppLoad già installato e la selezione a mano libera con il Marker. | Ripeti installazione, aggiornamento, uscita e scadenza della sessione con il pacchetto di rilascio. Controlla leggibilità e selezione con il Marker sul dispositivo. |
| Riconoscimento di pagine miste | Le prove automatiche coprono i contorni a mano libera, gli oggetti modificabili e i collegamenti interni. Una prova isolata sul tablet ha verificato l’OCR locale e la creazione di un quaderno con le pagine collegate di diagramma e tabella. | Prova testo, posizionamento delle celle ed etichette su esempi rappresentativi. Controlla a mano scrittura poco leggibile, bordi delle griglie, punteggiatura e caratteri simili. |
| Tastiera | Il tablet ha ricevuto input da una tastiera Bluetooth HID simulata. In modalità tablet sono stati verificati il cambio di layout della tastiera a schermo e il salvataggio della scelta. | Verifica abbinamento, digitazione e riconnessione con una tastiera Bluetooth fisica. |

## Pacchetti scaricabili

Per ogni pacchetto binario registra il commit sorgente, gli strumenti di compilazione, il sistema o firmware di destinazione e le verifiche svolte sulla build esatta. Quando pubblichi una nuova build, usa un nuovo tag di versione e nuovi checksum.

- **Paper Pro.** Compila con l’SDK `ferrari` corrispondente. Lo ZIP base contiene l’app e gli script di installazione. La voce di menu opzionale usa XOVI e AppLoad compatibili già installati sul tablet. Per WritePad includi motore, dizionari, avvisi di copyright e licenza e l’archivio dei sorgenti corrispondenti. Per l’OCR su immagini includi motore, modello e dizionario per l’alfabeto latino, ONNX Runtime e i relativi avvisi. [Compilazione e pacchetto Paper Pro](SVILUPPO.md#paper-pro) · [Sorgenti e pacchetto WritePad](../tools/writepad/README.md) · [Motore OCR per immagini](../tools/raster-ocr/README.md)
- **Mac.** L’archivio locale usa una firma ad hoc. Per distribuirlo direttamente, prepara la firma Developer ID e la notarizzazione e verifica il pacchetto firmato su un Mac pulito. Registra framework, librerie e plugin inclusi. Includi gli avvisi di licenza applicabili e l’accesso al codice sorgente Qt corrispondente, comprese eventuali patch. [Notarizzazione Apple](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution) · [Obblighi delle licenze Qt](https://www.qt.io/development/open-source-lgpl-obligations)

Controlla ogni ZIP finale con `unzip -tq <archivio.zip>`. Esegui `shasum -a 256 <archivio.zip>` e includi il risultato accanto al download. Se distribuisci il componente di trascrizione, includi anche lo SHA-256 dell’archivio dei sorgenti corrispondenti. Associa i binari al commit usato per compilarli. [GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)
