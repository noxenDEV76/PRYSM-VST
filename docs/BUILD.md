# Compilation reproductible

Versions épinglées : JUCE 8.0.15 (commit `91ad83ae34a81e0833b1a2b0866f54846370ae53`), CMake ≥ 3.22, NSIS 3.x, Visual Studio 2022 (MSVC) pour Windows.

## 1. Via GitHub Actions (recommandé pour l'EXE)
1. Créer un dépôt, y pousser ce dossier (sans `build*/` ni `third_party/JUCE`, déjà dans `.gitignore`).
2. Onglet Actions → workflow `build` (se lance au push, ou « Run workflow »).
3. Le job `windows` : récupère JUCE (vérification du commit), compile en Release x64, exécute les tests (y compris le test hôte VST3), génère l'EXE, vérifie son existence, l'installe en silencieux, relance le test hôte sur le plugin installé, le désinstalle, puis publie l'artefact **uniquement si tout a réussi**.

## 2. Windows en local
```powershell
git clone --branch 8.0.15 --depth 1 https://github.com/juce-framework/JUCE.git third_party/JUCE
.\scripts\build-windows.ps1
```
(ou `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` puis `cmake --build build --config Release`).
Sortie : `build/PRYSM_artefacts/Release/VST3/PRYSM.vst3`.

## 3. Reconstruire l'installateur
Installer NSIS 3, puis :
```powershell
.\scripts\build-installer.ps1 -Vst3Dir build/PRYSM_artefacts/Release/VST3/PRYSM.vst3 -OutDir dist -Version 1.0.0
```
(Linux/Wine : `scripts/build-installer.sh`.) Sortie : `dist/PRYSM-1.0.0-Windows-x64-Setup.exe`.

## 4. Linux (développement et tests)
```bash
./scripts/build-linux.sh
```
Dépendances : voir le workflow (`ninja-build`, `libasound2-dev`, `libfreetype-dev`, `libx11-dev`…, `xvfb`).

## Options CMake
`PRYSM_BUILD_TESTS` (ON), `PRYSM_ENABLE_LTO`, `PRYSM_VST3_MIDI_CC_PARAMS`.
