# Fonctionnalités de PRYSM 1.0.0

## Moteur
- Sampler chromatique, interpolation d'Hermite, position fractionnaire.
- Formats : WAV et AIFF (décodage hors du thread audio). Échec de chargement : l'échantillon précédent est conservé, message d'erreur explicite.
- Polyphonie 1–16 voix (32 emplacements physiques, vol de voix avec fondu de 4 ms), vélocité (sensibilité réglable), volume, pan.
- Modes : One-Shot, Gate. REVERSE. Start / End réglables (marqueurs déplaçables).
- ADSR, anti-clic (fondus d'entrée/sortie, lissage des paramètres), pédale de sustain, pitch bend (PITCH RANGE), all-notes-off.
- Remplacement d'échantillon sans détruire un buffer en cours de lecture (partage immuable, libération hors thread audio).

## Pitch Engine
ROOT NOTE (C5 = MIDI 60, convention FL), TUNE, PITCH RANGE, TRANSPOSE, PLAYBACK MODE, REVERSE.

## Envelope Lab
ADSR avec graphe interactif (poignées déplaçables).

## Mutation Engine (tous réellement appliqués à l'audio)
FILTER (LP/HP, cutoff, résonance), DRIVE (tanh suréchantillonné ×2, compensé), STEREO (largeur M/S, compatible mono), MOTION (LFO par voix vers filtre/pitch/amp/pan), RANDOM (déterministe, graine réglable), REVERB (mix/taille), TAIL (fondu en fin de région, distinct du release et de la reverb).

## Interface
En-tête (logo, état du moteur, nom de l'échantillon, activité), Sample Core (forme d'onde, glisser-déposer, durée/SR/canaux, marqueurs, playhead), Voice Monitor, clavier à l'écran. Fenêtre redimensionnable (ratio fixe), option REDUCE ANIMATIONS, infobulles.

## État / projet
Sauvegarde et restauration complètes des paramètres + chemin de l'échantillon + taille de fenêtre. Fichier manquant : message « SAMPLE NOT FOUND », aucun autre fichier substitué, chemin d'origine conservé.

## Limites connues
- Sortie stéréo uniquement. Pas de loop points, pas de time-stretch.
- ~2080 paramètres « MIDI CC » cachés exposés par le wrapper VST3 de JUCE (option CMake `PRYSM_VST3_MIDI_CC_PARAMS`). Leur effet dans FL Studio n'est pas vérifié.
