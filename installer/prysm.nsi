; =============================================================================
;  PRYSM - Windows installer (NSIS 3, Unicode, MUI2)
;
;  Build:   makensis /DVST3_DIR=<path to PRYSM.vst3 bundle> /DOUT_DIR=<output dir> installer\prysm.nsi
;  Install: <Program Files>\Common Files\VST3\PRYSM.vst3  (standard VST3 location, all users)
;  Silent:  PRYSM-<version>-Windows-x64-Setup.exe /S        (uninstall: "<Program Files>\PRYSM\Uninstall.exe" /S)
; =============================================================================
Unicode true
!ifndef VERSION
  !define VERSION "1.1.0"
!endif
!ifndef VST3_DIR
  !error "Define VST3_DIR: makensis /DVST3_DIR=path\to\PRYSM.vst3 installer\prysm.nsi"
!endif
!ifndef OUT_DIR
  !define OUT_DIR "."
!endif

; Fail the build (never ship an empty installer) if the payload is missing.
!if /FileExists "${VST3_DIR}\Contents\x86_64-win\PRYSM.vst3"
!else
  !error "Missing ${VST3_DIR}\Contents\x86_64-win\PRYSM.vst3 - build the plugin first."
!endif

!define APP_NAME      "PRYSM"
!define PUBLISHER     "PRYSM Audio"
!define UNINST_KEY    "Software\Microsoft\Windows\CurrentVersion\Uninstall\PRYSM"
!define REG_KEY       "Software\PRYSM Audio\PRYSM"

Name "${APP_NAME} ${VERSION}"
OutFile "${OUT_DIR}/PRYSM-${VERSION}-Windows-x64-Setup.exe"
RequestExecutionLevel admin
InstallDir "$PROGRAMFILES64\PRYSM"
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUninstDetails show
BrandingText "${APP_NAME} ${VERSION}"

VIProductVersion "${VERSION}.0"
; (the version resource is declared after the language macros below)

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_TITLE_3LINES

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "../docs/LICENSE-NOTICE.txt"   ; relative to this script
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "English"

!macro PRYSM_VERSIONINFO LANGID
  VIAddVersionKey /LANG=${LANGID} "ProductName"     "${APP_NAME}"
  VIAddVersionKey /LANG=${LANGID} "CompanyName"     "${PUBLISHER}"
  VIAddVersionKey /LANG=${LANGID} "LegalCopyright"  "AGPL-3.0-or-later"
  VIAddVersionKey /LANG=${LANGID} "FileDescription" "${APP_NAME} VST3 installer"
  VIAddVersionKey /LANG=${LANGID} "FileVersion"     "${VERSION}"
  VIAddVersionKey /LANG=${LANGID} "ProductVersion"  "${VERSION}"
!macroend
!insertmacro PRYSM_VERSIONINFO ${LANG_ENGLISH}
!insertmacro PRYSM_VERSIONINFO ${LANG_FRENCH}

; ---- localized messages -----------------------------------------------------
LangString MSG_NEED_X64    ${LANG_FRENCH}  "PRYSM est un plugin 64 bits : il necessite Windows 64 bits."
LangString MSG_NEED_X64    ${LANG_ENGLISH} "PRYSM is a 64-bit plugin and requires 64-bit Windows."
LangString MSG_NEED_ADMIN  ${LANG_FRENCH}  "Les droits administrateur sont requis pour installer dans le dossier VST3 commun. Relancez l'installateur en tant qu'administrateur."
LangString MSG_NEED_ADMIN  ${LANG_ENGLISH} "Administrator rights are required to install into the shared VST3 folder. Please run the installer as administrator."
LangString MSG_LOCKED      ${LANG_FRENCH}  "Le fichier du plugin est utilise par un autre programme (FL Studio ou un autre hote est probablement ouvert).$\r$\n$\r$\nFermez-le puis cliquez sur Reessayer."
LangString MSG_LOCKED      ${LANG_ENGLISH} "The plugin file is in use by another program (FL Studio or another host is probably open).$\r$\n$\r$\nClose it, then click Retry."
LangString MSG_VERIFY_FAIL ${LANG_FRENCH}  "Verification impossible : le fichier installe est introuvable ou vide :$\r$\n$0$\r$\nL'installation a echoue."
LangString MSG_VERIFY_FAIL ${LANG_ENGLISH} "Verification failed: the installed file is missing or empty:$\r$\n$0$\r$\nThe installation failed."
LangString MSG_COPY_FAIL   ${LANG_FRENCH}  "Impossible de copier les fichiers du plugin dans :$\r$\n$0$\r$\nVerifiez les droits d'acces et l'espace disque."
LangString MSG_COPY_FAIL   ${LANG_ENGLISH} "Could not copy the plugin files to:$\r$\n$0$\r$\nCheck permissions and free disk space."
LangString MSG_DONE        ${LANG_FRENCH}  "PRYSM est installe. Dans FL Studio : Options > Gestionnaire de plugins > Rechercher de nouveaux plugins."
LangString MSG_DONE        ${LANG_ENGLISH} "PRYSM is installed. In FL Studio: Options > Manage plugins > Find installed plugins."
LangString MSG_UN_LOCKED   ${LANG_FRENCH}  "Impossible de supprimer le plugin : il est utilise (fermez FL Studio) ou protege.$\r$\nFermez l'hote puis cliquez sur Reessayer."
LangString MSG_UN_LOCKED   ${LANG_ENGLISH} "Could not remove the plugin: it is in use (close FL Studio) or protected.$\r$\nClose the host, then click Retry."

!define VST3_TARGET "$COMMONFILES64\VST3\PRYSM.vst3"
!define DLL_REL     "Contents\x86_64-win\PRYSM.vst3"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "$(MSG_NEED_X64)"
    Abort
  ${EndIf}
  SetRegView 64
  UserInfo::GetAccountType
  Pop $1
  ${If} $1 != "admin"
    MessageBox MB_OK|MB_ICONSTOP "$(MSG_NEED_ADMIN)"
    Abort
  ${EndIf}
FunctionEnd

Section "PRYSM VST3" SecMain
  SectionIn RO
  SetRegView 64

  ; 1) Replace a previous version (retry while the host still has the DLL loaded)
  retry_replace:
  ClearErrors
  IfFileExists "${VST3_TARGET}\${DLL_REL}" 0 do_copy
    Delete "${VST3_TARGET}\${DLL_REL}"
    ${If} ${Errors}
      MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "$(MSG_LOCKED)" IDRETRY retry_replace
      Abort
    ${EndIf}
  do_copy:
  RMDir /r "${VST3_TARGET}"

  ; 2) Copy the bundle
  ClearErrors
  SetOutPath "${VST3_TARGET}"
  File /r "${VST3_DIR}\*.*"
  ${If} ${Errors}
    StrCpy $0 "${VST3_TARGET}"
    MessageBox MB_OK|MB_ICONSTOP "$(MSG_COPY_FAIL)"
    Abort
  ${EndIf}

  ; 3) Verify what was installed
  ClearErrors
  FileOpen $2 "${VST3_TARGET}\${DLL_REL}" r
  ${If} ${Errors}
    StrCpy $0 "${VST3_TARGET}\${DLL_REL}"
    MessageBox MB_OK|MB_ICONSTOP "$(MSG_VERIFY_FAIL)"
    Abort
  ${EndIf}
  FileSeek $2 0 END $3
  FileClose $2
  ${If} $3 < 100000
    StrCpy $0 "${VST3_TARGET}\${DLL_REL}"
    MessageBox MB_OK|MB_ICONSTOP "$(MSG_VERIFY_FAIL)"
    Abort
  ${EndIf}

  ; 4) Uninstaller + Add/Remove Programs entry
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayName"     "${APP_NAME} (VST3)"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayVersion"  "${VERSION}"
  WriteRegStr   HKLM "${UNINST_KEY}" "Publisher"       "${PUBLISHER}"
  WriteRegStr   HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr   HKLM "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegStr   HKLM "${UNINST_KEY}" "InstallLocation" "${VST3_TARGET}"
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
  ${GetSize} "${VST3_TARGET}" "/S=0K" $4 $5 $6
  WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" $4
  WriteRegStr   HKLM "${REG_KEY}" "Version" "${VERSION}"
  WriteRegStr   HKLM "${REG_KEY}" "VST3Path" "${VST3_TARGET}"

  DetailPrint "$(MSG_DONE)"
SectionEnd

Section "Uninstall"
  SetRegView 64
  retry_remove:
  ClearErrors
  IfFileExists "${VST3_TARGET}\${DLL_REL}" 0 do_rm
    Delete "${VST3_TARGET}\${DLL_REL}"
    ${If} ${Errors}
      MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "$(MSG_UN_LOCKED)" IDRETRY retry_remove
      Abort
    ${EndIf}
  do_rm:
  RMDir /r "${VST3_TARGET}"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "${UNINST_KEY}"
  DeleteRegKey HKLM "${REG_KEY}"
  DeleteRegKey /ifempty HKLM "Software\PRYSM Audio"
SectionEnd
