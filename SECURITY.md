# Security

**English** · [Italiano](SECURITY.it.md)

Quaderno is in preview. Include the commit or build on which you reproduced a security issue. For workflows involving SSH or the system interface, also include the firmware version and the shortest set of steps to reproduce it.

## Reporting a vulnerability

Use **Report a vulnerability** under the repository’s **Security** tab when that option is available. Otherwise, ask the maintainer for a private channel. You can open an issue to request contact details while keeping technical details of the vulnerability private. [Private reporting on GitHub](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/report-privately)

Use sample data and omit credentials, PIN configuration files, and personal library content. Include the impact you observed, the steps to reproduce it, and any relevant log excerpts.

## Data and privileges

The Paper Pro trial starts over SSH as `root` and runs as a temporary systemd service. The PIN controls access through the app interface. Quaderno stores its library in a JSON file. [Installation](docs/en/INSTALLATION.md) · [Data, backups, and privacy](docs/en/DATA.md)
