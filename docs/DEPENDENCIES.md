# Dépendances et licences

| Composant | Version / source | Licence | Usage |
|---|---|---|---|
| JUCE | 8.0.15, commit `91ad83ae34a81e0833b1a2b0866f54846370ae53` (https://github.com/juce-framework/JUCE) | AGPLv3 (ou licence commerciale JUCE) | Framework audio / GUI / VST3 |
| SDK VST 3 de Steinberg | embarqué dans JUCE 8.0.15 | MIT | Interface VST3 |
| Décodeurs FLAC, Ogg Vorbis | embarqués dans JUCE | BSD-3-Clause | Formats FLAC / OGG |
| Orbitron | Google Fonts (`ofl/orbitron`), The League of Moveable Type | SIL OFL 1.1 (`assets/fonts/OFL-Orbitron.txt`) |
| Rajdhani SemiBold | Google Fonts (`ofl/rajdhani`), Indian Type Foundry | SIL OFL 1.1 (`assets/fonts/OFL-Rajdhani.txt`) | Titres, libellés |
| Share Tech Mono Regular | Google Fonts (`ofl/sharetechmono`), Carrois Type Design | SIL OFL 1.1 (`assets/fonts/OFL-ShareTechMono.txt`) | Valeurs numériques |
| CMake | ≥ 3.22 | BSD-3-Clause | Build |
| NSIS | 3.x (3.09 testé) | zlib/libpng | Installateur |
| Visual Studio 2022 (MSVC) | runner `windows-2022` | licence Microsoft | Compilation Windows (CI) |
| GitHub Actions | `actions/checkout` v4.2.2, `actions/setup-python` v5.6.0, `actions/upload-artifact` v4.6.2 (épinglées par SHA de commit) | MIT | CI |

## Licence de PRYSM — point important

JUCE est distribué sous **AGPLv3** ou sous licence commerciale. PRYSM a donc été publié sous **AGPL-3.0-or-later** (fichier `LICENSE`) :
vous pouvez le copier, le modifier et le redistribuer, à condition de fournir le code source correspondant et de conserver cette licence.
**Vendre PRYSM sous forme fermée (sans publier le code) exige une licence commerciale JUCE.**
Ce n'est pas un conseil juridique : vérifiez les conditions sur https://juce.com/legal/juce-8-licence/.

« VST » est une marque de Steinberg Media Technologies GmbH.

## Aucune dépendance réseau à l'exécution
Le plugin ne se connecte à aucun serveur (`JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0`). Polices embarquées dans le binaire.
