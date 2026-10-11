# PRYSM — Alien Sample Lab (VST3)

Sampler VST3 chromatique : on dépose un one-shot, on le joue depuis le piano roll (FL Studio ou tout hôte VST3), puis on le mute avec le Mutation Engine (filtre, drive, stéréo, motion, random, reverb, tail).

Interface entièrement en anglais, documentation en français. Licence : AGPL-3.0-or-later (voir `docs/DEPENDENCIES.md`).

## État réel de la livraison (lire avant tout)

| Élément | État |
|---|---|
| Code source complet | Fourni |
| Compilation Linux + tests | Réussie (voir `docs/TESTS.md`) |
| Compilation Windows 64 bits (MSVC) + tests, via GitHub Actions | **Réussie** (run vert, voir Actions) |
| EXE d'installation | **Produit par la CI** et publié dans la Release `v1.0.0` ; installation/désinstallation silencieuses vérifiées sur le runner Windows |
| Test dans FL Studio | **Non réalisé** |
| Signature de l'EXE | Non signé (avertissement SmartScreen attendu) |

Pour reconstruire l'EXE : pousser ce dossier dans un dépôt GitHub, lancer le workflow `build` ; l'artefact `PRYSM-1.0.0-windows-x64` contient `PRYSM-1.0.0-Windows-x64-Setup.exe` et un zip du VST3. Détails : `docs/BUILD.md`.

## Documentation

- `docs/INSTALLATION.md` — installation, FL Studio, désinstallation, dépannage
- `docs/FONCTIONNALITES.md` — fonctionnalités
- `docs/TESTS.md` — tests réellement exécutés, et ce qui n'a PAS été testé
- `docs/BUILD.md` — compilation reproductible, reconstruction de l'installateur
- `docs/DEPENDENCIES.md` — dépendances, versions, licences
