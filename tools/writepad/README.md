# Quaderno's WritePad helper

This standalone Linux helper uses PhatWare's WritePad Handwriting Recognition Engine and Italian and English dictionaries to transcribe selected pen strokes locally on reMarkable Paper Pro. Quaderno communicates with it over standard input and output. The executable accepts an array of strokes containing normalized `[x, y, pressure]` points and prints a JSON object with `text`, `alternatives` (up to three engine candidates), or `error`. It uses `libjson-c`, which is supplied by Paper Pro firmware 3.29.1.0.

The helper source is licensed under [GPL-3.0-or-later](COPYING). The WritePad source, Italian dictionary, copyright notice and license are available at the pinned revision below. [WritePad source and license](https://github.com/phatware/WritePad-Handwriting-Recognition-Engine/tree/d6cd5b032d679e739e427a6d03d03a6219b0dcaf)

On Linux, source the `environment-setup-cortexa53-crypto-remarkable-linux` script from the matching reMarkable SDK first. Run the following commands from the Quaderno repository root.

```sh
curl --fail --location \
  https://codeload.github.com/phatware/WritePad-Handwriting-Recognition-Engine/tar.gz/d6cd5b032d679e739e427a6d03d03a6219b0dcaf \
  --output writepad-source.tar.gz
sh tools/writepad/build.sh writepad-source.tar.gz build-paper-pro/Quaderno-Paper-Pro-3.29.1.0
```

The build checks the archive's SHA-256 and compiles the Italian and English recognizers. Put `quaderno-handwriting` and both `quaderno-handwriting-it.dct` and `quaderno-handwriting-en.dct` beside the main `quaderno` executable. The build also emits the WritePad license and copyright notice. Run `/path/to/quaderno-handwriting --check` and `/path/to/quaderno-handwriting --check --language en` on the tablet to verify both dictionaries. The default language remains Italian. Pass `--language en` to recognize English strokes. Language tables for both recognizers are compiled into the helper; changing only a dictionary file is insufficient.

The build uses WritePad’s synchronous recognition path to avoid a missed worker wakeup. Quaderno enables `QUADERNO_HANDWRITING_PROSE=1` for text transcription. In that mode the helper can prefer a dictionary candidate already returned by WritePad when it resolves a final `0` to `O` in an isolated word. The original transcript stays available as an alternative. The default mode preserves the engine's top result for cells and diagram labels.

Run the synthetic recognition checks on a system that can execute the built helper and access its dictionaries.

```sh
python3 scripts/check-handwriting.py --engine writepad /path/to/quaderno-handwriting
```

To prepare a preview ZIP that includes the helper and a matching source archive, run the packager from the same SDK environment and choose a fresh output directory.

```sh
sh scripts/package-paper-pro.sh build-paper-pro/quaderno build-paper-pro/release writepad-source.tar.gz
```

Distribute `Quaderno-WritePad-Source.tar.gz` alongside the ZIP with the helper. It contains the pinned upstream archive, the helper source, the build script and the license notices. To rebuild just the helper from that archive, extract it and run `sh tools/writepad/build.sh writepad-source.tar.gz OUTPUT_DIRECTORY` from its root with the matching SDK.
