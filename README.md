# PRYSM — Alien Sample Lab (VST3)

Sampler VST3 chromatique : on dépose un one-shot, on le joue depuis le piano roll (FL Studio ou tout hôte VST3), puis on le mute avec le Mutation Engine (filtre, drive, stéréo, motion, random, reverb, tail).

Interface entièrement en anglais, documentation en français. Licence : AGPL-3.0-or-later (voir `docs/DEPENDENCIES.md`).

## État réel de la livraison (lire avant tout)

| Élément | État |
|---|---|
| Code source complet (moteur, UI, tests, CMake) | Fourni |
| Compilation Linux + tests | Réalisée et réussie ici (voir `docs/TESTS.md`) |
| **VST3 Windows 64 bits compilé** | **Non produit ici** : JUCE refuse MinGW et MSVC n'est pas disponible dans mon environnement. Il est produit par le workflow GitHub Actions `windows-2022` (MSVC) fourni. |
| **EXE d'installation** | **Non produit ici** pour la même raison (il embarque le VST3 Windows). Script NSIS + pipeline fournis ; mécanique vérifiée sous Wine avec une charge factice uniquement. |
| Test dans FL Studio | **Non réalisé** (FL Studio indisponible). |

Pour obtenir l'EXE : pousser ce dossier dans un dépôt GitHub, lancer le workflow `build` ; l'artefact `PRYSM-1.1.0-windows-x64` contient `PRYSM-1.1.0-Windows-x64-Setup.exe` et un zip du VST3. Détails : `docs/BUILD.md`.

## Documentation

- `docs/INSTALLATION.md` — installation, FL Studio, désinstallation, dépannage
- `docs/FONCTIONNALITES.md` — fonctionnalités
- `docs/TESTS.md` — tests réellement exécutés, et ce qui n'a PAS été testé
- `docs/BUILD.md` — compilation reproductible, reconstruction de l'installateur
- `docs/DEPENDENCIES.md` — dépendances, versions, licences
