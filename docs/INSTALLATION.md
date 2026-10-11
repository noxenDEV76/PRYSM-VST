# Installation de PRYSM (Windows 64 bits)

## 1. Installer avec l'installateur EXE (recommandé)

1. Récupérez `PRYSM-1.0.0-Windows-x64-Setup.exe` (voir « D'où vient l'EXE ? » ci-dessous).
2. Double-cliquez dessus. Windows demande les droits administrateur : acceptez.
   - Si SmartScreen affiche « Windows a protégé votre ordinateur » : l'installateur n'est pas signé numériquement
     (une signature de code est un certificat payant). Cliquez sur **Informations complémentaires → Exécuter quand même**.
3. Suivez l'assistant (français ou anglais selon votre Windows). Le plugin est copié dans :

   `C:\Program Files\Common Files\VST3\PRYSM.vst3`

   C'est le dossier VST3 standard, déjà analysé par FL Studio.
4. Si FL Studio (ou un autre hôte) est ouvert et utilise déjà PRYSM, l'installateur vous demande de le fermer
   puis de cliquer sur **Réessayer**.

Installation silencieuse (déploiement) : `PRYSM-1.0.0-Windows-x64-Setup.exe /S`

## 2. Installer à la main (alternative)

Décompressez `PRYSM-1.0.0-VST3-Windows-x64.zip` et copiez le dossier `PRYSM.vst3` dans
`C:\Program Files\Common Files\VST3\` (droits administrateur nécessaires).

## 3. Premier lancement dans FL Studio

1. **Options → Gestionnaire de plugins** (*Manage plugins*), puis **Rechercher de nouveaux plugins** (*Find installed plugins*).
   Vérifiez que `C:\Program Files\Common Files\VST3` figure dans la liste des dossiers analysés.
2. Dans le Channel Rack : **+ → PRYSM** (liste des plugins installés, catégorie instruments / générateurs).
3. Glissez un one-shot (WAV, AIFF, FLAC ou OGG) depuis l'Explorateur Windows dans la zone
   **DROP A SAMPLE TO BEGIN**, ou cliquez sur **LOAD SAMPLE**.
4. Jouez avec le clavier MIDI, le piano roll ou le clavier affiché dans PRYSM.

### Repères utiles
- **ROOT NOTE** : la touche qui joue le sample à sa hauteur d'origine. Convention FL Studio : MIDI 60 = **C5**.
- **ONE-SHOT** : le sample est joué jusqu'au bout, le relâchement de la touche est ignoré.
  **GATE** : le son s'arrête (avec RELEASE) quand la touche est relâchée.
- **TAIL** (fondu de fin de zone), **RELEASE** (enveloppe, mode GATE) et **REVERB** (queue d'effet) sont trois réglages distincts.
- Les paramètres sont automatisables (clic droit sur un bouton → *Create automation clip*, ou via la liste des paramètres du plugin).
- Le projet mémorise le **chemin** du sample, pas une copie. Si le fichier est déplacé ou supprimé, PRYSM affiche
  **SAMPLE NOT FOUND** et reste muet : déposez le fichier ou utilisez **LOCATE SAMPLE**. Aucun autre fichier n'est
  substitué automatiquement.
- Animations : engrenage (en haut à droite) → **REDUCE ANIMATIONS**.
- Fenêtre redimensionnable : tirez le coin inférieur droit (proportions conservées).

## 4. Désinstaller

Paramètres Windows → **Applications → PRYSM (VST3) → Désinstaller**
(ou `C:\Program Files\PRYSM\Uninstall.exe`). Le dossier `PRYSM.vst3`, le désinstalleur et les entrées de registre sont supprimés.

## 5. D'où vient l'EXE ?

L'EXE et le VST3 Windows sont produits par le workflow GitHub Actions `.github/workflows/build.yml`
(runner Windows, compilateur MSVC, build Release). Voir `docs/BUILD.md` :
poussez le dépôt sur GitHub, ouvrez l'onglet **Actions**, lancez/attendez le workflow **build**, puis téléchargez
l'artefact `PRYSM-1.0.0-windows-x64`. L'artefact n'est publié que si la compilation, les tests, la création
de l'installateur et un test d'installation/désinstallation silencieux ont tous réussi.

## 6. Dépannage

| Symptôme | Piste |
|---|---|
| PRYSM absent de FL Studio | Relancer *Rechercher de nouveaux plugins* ; vérifier le dossier VST3 ci-dessus. |
| « SAMPLE NOT FOUND » | Le fichier a été déplacé : le déposer à nouveau ou **LOCATE SAMPLE**. |
| `UNSUPPORTED OR CORRUPT FILE` | Format non lisible. Formats pris en charge : WAV, AIFF, FLAC, OGG. L'ancien sample est conservé. |
| Voyant **OVERLOAD** rouge | Le niveau de sortie a dépassé 0 dBFS : baisser VOLUME / DRIVE (clic sur le moniteur pour effacer). |
| FL Studio liste beaucoup d'entrées « MIDI CC … » | Comportement standard de JUCE (VST3 `IMidiMapping`). Reconstruire avec `-DPRYSM_VST3_MIDI_CC_PARAMS=OFF` si votre hôte les affiche tout en livrant bien la molette de pitch et la pédale. |
