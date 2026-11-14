# Sicurezza

[English](SECURITY.md) · **Italiano**

Quaderno è in fase di anteprima. Per le verifiche di sicurezza indica il commit o la build su cui hai riprodotto il problema. Per i flussi che usano SSH o l’interfaccia di sistema includi anche la versione del firmware e i passaggi minimi.

## Segnalare una vulnerabilità

Se nella scheda **Security** del repository è disponibile **Report a vulnerability**, usa quel canale privato. Altrimenti chiedi al manutentore un canale privato. Puoi usare una Issue per chiedere il contatto, omettendo i dettagli tecnici della vulnerabilità. [Segnalazione privata su GitHub](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/report-privately)

Usa dati di esempio e ometti credenziali, file di configurazione del PIN e contenuti della tua libreria. Includi l’impatto osservato, i passaggi per riprodurlo e gli eventuali log ridotti al minimo necessario.

## Dati e privilegi

La prova sul Paper Pro viene avviata tramite SSH come `root` e gestita da un servizio systemd temporaneo. Il PIN protegge l’accesso all’interfaccia e la libreria viene salvata in un file JSON. Per i dettagli su backup, accesso e protezione del dispositivo consulta le guide. [Installazione](docs/INSTALLAZIONE.md) · [Dati, backup e privacy](docs/DATI.md)
