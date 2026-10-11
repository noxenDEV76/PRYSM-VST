# Tests

Ce document ne liste que ce qui a **réellement été exécuté**, avec le résultat observé.

## Exécutés (Linux x86-64, GCC en Release « dev », sous Xvfb pour l'éditeur)

CTest : 3/3 réussis (dernier passage : engine_and_processor 1,9 s ; make_test_sample ; vst3_host 3,1 s).

1. **engine_and_processor — 105/105 vérifications** : décodage WAV/AIFF et erreurs (fichier absent, vide, corrompu, sans audio…) ; justesse de hauteur (octaves, ROOT/TUNE/TRANSPOSE/pitch bend) ; modes One-Shot/Gate et enveloppe ADSR ; Start/End/Reverse ; polyphonie et vol de voix ; remplacement d'échantillon pendant la lecture ; effets (LP/HP, DRIVE, STEREO, MOTION, RANDOM déterministe/bypass à 0, REVERB, TAIL) ; processeur : échec de chargement conserve l'échantillon précédent ; sauvegarde/restauration d'état ; fichier manquant (statut Missing, chemin conservé, aucune substitution) ; éditeur ouvert/fermé 5 fois + captures d'écran headless.
2. **vst3_host — 16/16 vérifications** sur le **binaire .vst3 Linux réellement compilé** : chargement par le module VST3, 24 paramètres PRYSM nommés, bus stéréo, rendu MIDI, état sauvegardé/restauré dans une 2ᵉ instance.
3. **Build clang (Linux, Release)** : compilation complète réussie (90/90), avec avertissements de style non bloquants.
4. **Installateur NSIS sous Wine** avec une **charge factice** (pas le vrai plugin) : installation, entrées de registre, désinstallation propre.
5. **Workflow YAML** : syntaxe validée (non exécuté sur GitHub).

## NON réalisé — à ne pas considérer comme vérifié

- Compilation MSVC / Windows 64 bits (impossible ici : JUCE rejette MinGW). Le workflow est écrit mais **jamais exécuté**.
- VST3 Windows réel et **EXE d'installation réel** (donc non testé : installation sur Windows, élévation admin, SmartScreen, fichier verrouillé, utilisateur non-admin).
- **FL Studio** : aucun test (logiciel indisponible), y compris l'effet des ~2080 paramètres MIDI CC cachés.
- Écoute subjective / qualité sonore sur casque, et mesures de CPU en conditions réelles.
- Build LTO Release final (le build dev utilise `PRYSM_ENABLE_LTO=OFF`).
