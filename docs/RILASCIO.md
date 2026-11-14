# Preparazione del rilascio

[English](en/RELEASE.md) · **Italiano**

## Anteprima dei sorgenti 0.1.0

L’anteprima include Quaderno, gli script di installazione e le guide in inglese e italiano. Quaderno usa la licenza MIT. Il componente opzionale WritePad ha una licenza separata GPL-3.0-or-later. [Licenza](../LICENSE) · [Componenti di terze parti](../THIRD_PARTY_NOTICES.md) · [Cronologia](../CHANGELOG.it.md)

Prima di rendere pubblico il repository, rivedi i file e i metadati dei commit, oltre alla visibilità delle issue, dei log delle verifiche automatiche e degli allegati. Usa dati di esempio nelle schermate e nei log. Esegui compilazione e prova di avvio sul commit definitivo e usa quel commit per il tag dell’anteprima. [Guida di sviluppo](SVILUPPO.md) · [Visibilità dei repository GitHub](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility)

## Verifiche

| Piattaforma o flusso | Verifiche eseguite | Verifiche per un pacchetto scaricabile |
| --- | --- | --- |
| Mac Apple Silicon | Compilazione e avvio. La trascrizione locale è stata verificata con il componente per macOS. | Prova il pacchetto firmato su un Mac pulito. Verifica la versione minima di macOS, l’importazione dei file e il recupero della libreria. |
| reMarkable Paper Pro 3.29.1.0 | Compilazione ARM64, avvio da AppLoad, eventi nativi del Marker, aggiornamenti regionali durante la navigazione e refresh completo manuale. | Ripeti installazione, aggiornamento, uscita e scadenza della sessione con il pacchetto di rilascio. Controlla la leggibilità sul dispositivo. |
| Tastiera | Il tablet ha ricevuto input da una tastiera Bluetooth HID simulata. In modalità tablet sono stati verificati il cambio di layout della tastiera a schermo e il salvataggio della scelta. | Verifica abbinamento, digitazione e riconnessione con una tastiera Bluetooth fisica. |

## Pacchetti scaricabili

La distribuzione dei binari è una fase separata dall’anteprima dei sorgenti. Per ogni pacchetto registra il commit sorgente, gli strumenti di compilazione, il sistema o firmware di destinazione e le verifiche svolte sulla build esatta.

- **Paper Pro.** Compila con l’SDK `ferrari` corrispondente. Lo ZIP base contiene l’app e gli script di installazione. L’edizione opzionale con WritePad richiede il motore, il dizionario, gli avvisi di licenza e l’archivio dei sorgenti corrispondenti insieme al binario. Il dispositivo fornisce firmware e runtime Qt. [Compilare e preparare il pacchetto Paper Pro](SVILUPPO.md#paper-pro) · [Sorgenti e pacchetto WritePad](../tools/writepad/README.md)
- **Mac.** L’archivio locale usa una firma ad hoc. Per distribuirlo direttamente, prepara la firma Developer ID e la notarizzazione e verifica il pacchetto firmato su un Mac pulito. Registra framework, librerie e plugin inclusi. Includi gli avvisi di licenza applicabili e l’accesso al codice sorgente Qt corrispondente, comprese eventuali patch. [Notarizzazione Apple](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution) · [Obblighi delle licenze Qt](https://www.qt.io/development/open-source-lgpl-obligations)

Controlla ogni ZIP finale con `unzip -tq <archivio.zip>`. Esegui `shasum -a 256 <archivio.zip>` e includi il risultato accanto al download. Se distribuisci il componente di trascrizione, includi anche lo SHA-256 dell’archivio dei sorgenti corrispondenti. Associa i binari al commit usato per compilarli. [GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)
