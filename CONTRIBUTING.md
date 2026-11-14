# Contributing to Quaderno

**English** · [Italiano](CONTRIBUTING.it.md)

Use the repository’s **Issues** tab to suggest a change or report a reproducible bug. For sensitive reports, follow the security policy. [Security policy](SECURITY.md)

## Reporting a bug

Include the build or commit and operating system. For Paper Pro, include the tablet model and full firmware version. Describe how to reproduce the issue, what you expected, and what happened. State whether you tested on a physical tablet or in a simulated environment.

If an attachment helps, use a sample notebook with the smallest amount of content needed to reproduce the issue. Remove credentials, personal documents, serial numbers, and network details from screenshots and logs.

## Proposing a change

1. Create a branch from `main` and describe the change in your pull request.
2. Build Quaderno and verify your change. For QML changes, test a narrow window and tablet mode. For library changes, verify that existing files open and that saving and reopening preserves the data. [Development guide](docs/en/DEVELOPMENT.md)
3. In the pull request, list the checks you ran and any areas that still need testing. Update the relevant user or installation guide in both languages when a documented workflow changes.

Quaderno uses C++17 and Qt Quick. Commit source files, scripts, and documentation to the repository. Prepare distribution binaries as release packages. [Release preparation](docs/en/RELEASE.md)

Contributions use the license that applies to the files they change. Quaderno uses [MIT](LICENSE), and the optional WritePad helper uses GPL-3.0-or-later. [Third-party notices](THIRD_PARTY_NOTICES.md)
