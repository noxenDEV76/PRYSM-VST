# Fonctionnalités de PRYSM 1.1.0

PRYSM est volontairement simple : on dépose un sample, on le retouche un peu, on le joue au clavier MIDI.

## Interface (tout en anglais dans le plugin)
- Titre « PRYSM » seul, fond futuriste bleu néon avec fumée animée et particules (qui réagissent au niveau audio). Option REDUCE ANIMATIONS (roue crantée) : fond fixe, moins de CPU.
- **Sample** : glisser-déposer ou LOAD SAMPLE, forme d'onde, nom/durée/fréquence/canaux, marqueurs START / END déplaçables (double-clic = reset), curseur de lecture.
- **Envelope Lab** (style FL Studio) : graphe interactif + DELAY, ATT, HOLD, DEC, SUS, REL + 3 boutons TENSION (courbes d'attaque, de decay et de release). Points carrés = temps / niveau, points ronds = courbure. Double-clic sur un point = remise à zéro.
- **Pitch** : ROOT NOTE (C5 = MIDI 60, convention FL), FINE TUNE, ONE-SHOT / GATE, REVERSE.
- Clavier à l'écran (la touche racine est marquée, les notes MIDI de l'hôte s'allument).
- Fenêtre redimensionnable (proportions fixes).

## Moteur
- Sampler chromatique, interpolation d'Hermite, 16 voix max, vélocité, volume/pan (automatisables, sans bouton), pitch bend, pédale de sustain.
- WAV, AIFF (+ FLAC, OGG) décodés hors du thread audio ; échec de chargement = l'échantillon précédent est conservé.
- Enveloppe DAHDSR avec tensions, la courbe dessinée est exactement celle utilisée par le moteur. Anti-clic sur toutes les étapes.
- Remplacement d'échantillon sans interrompre les voix en cours ; fichier manquant signalé (« SAMPLE NOT FOUND »), jamais remplacé en silence.
- Sauvegarde/restauration complète du projet.

## Retiré par rapport à la 1.0 (demande de simplification)
Output, Pitch Engine (pitch range, transpose), Mutation Engine (filtre, drive, stéréo, LFO, random, reverb, tail), Voice Monitor, logo, noms « alien », indicateur d'état et jauge de volume. Pas de LFO. Volume, pan, pitch range, transpose, voices et vélocité restent disponibles comme paramètres d'automation de l'hôte (sans bouton dans l'interface).

## Limites connues
- Sortie stéréo uniquement ; pas de loop points ni de time-stretch.
- ~2080 paramètres « MIDI CC » cachés exposés par le wrapper VST3 de JUCE (option CMake `PRYSM_VST3_MIDI_CC_PARAMS`) ; effet dans FL Studio non vérifié.
