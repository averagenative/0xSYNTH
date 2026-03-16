; 0xSYNTH NSIS Installer Script
; Builds a Windows installer from Linux via makensis
;
; Usage: makensis scripts/packaging/0xsynth_installer.nsi

!include "MUI2.nsh"

!define VER "1.0.0"
!define VERFULL "1.0.0.0"

; --- General ---
Name "0xSYNTH v${VER}"
OutFile "../../release/0xSYNTH-${VER}-windows-x64-setup.exe"
InstallDir "$PROGRAMFILES64\0xSYNTH"
InstallDirRegKey HKLM "Software\0xSYNTH" "InstallDir"
RequestExecutionLevel admin

; --- Version info ---
VIProductVersion "${VERFULL}"
VIAddVersionKey "ProductName" "0xSYNTH"
VIAddVersionKey "CompanyName" "Dan Michael"
VIAddVersionKey "FileDescription" "0xSYNTH Multi-Engine Synthesizer"
VIAddVersionKey "FileVersion" "${VER}"
VIAddVersionKey "LegalCopyright" "MIT License"

; --- Interface ---
!define MUI_ABORTWARNING

; --- Pages ---
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; --- Upgrade detection ---
Function .onInit
    ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\0xSYNTH" "UninstallString"
    StrCmp $0 "" done

    MessageBox MB_OKCANCEL|MB_ICONINFORMATION \
        "0xSYNTH is already installed.$\n$\nClick OK to uninstall the previous version and continue, or Cancel to abort." \
        IDOK uninst
    Abort

uninst:
    ExecWait '"$0" /S _?=$INSTDIR'
    Delete "$INSTDIR\uninstall.exe"

done:
FunctionEnd

; --- Sections ---

Section "0xSYNTH Standalone (required)" SecMain
    SectionIn RO

    SetOutPath "$INSTDIR"
    File "..\..\build-win64\0xsynth.exe"
    File "..\..\README.md"
    File "..\..\LICENSE"
    File "..\..\FEATURES.md"

    ; Factory presets
    SetOutPath "$INSTDIR\presets\factory"
    File /r "..\..\presets\factory\*.*"

    ; Start Menu
    CreateDirectory "$SMPROGRAMS\0xSYNTH"
    CreateShortCut "$SMPROGRAMS\0xSYNTH\0xSYNTH.lnk" "$INSTDIR\0xsynth.exe"
    CreateShortCut "$SMPROGRAMS\0xSYNTH\Uninstall.lnk" "$INSTDIR\uninstall.exe"

    ; Desktop shortcut
    CreateShortCut "$DESKTOP\0xSYNTH.lnk" "$INSTDIR\0xsynth.exe"

    ; Registry
    WriteRegStr HKLM "Software\0xSYNTH" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\0xSYNTH" \
        "DisplayName" "0xSYNTH - Multi-Engine Synthesizer"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\0xSYNTH" \
        "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\0xSYNTH" \
        "DisplayVersion" "${VER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\0xSYNTH" \
        "Publisher" "Dan Michael"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\0xSYNTH" \
        "DisplayIcon" '"$INSTDIR\0xsynth.exe",0'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\0xSYNTH" \
        "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\0xSYNTH" \
        "NoRepair" 1

    ; Uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "CLAP Plugin" SecCLAP
    SetOutPath "$COMMONFILES64\CLAP"
    File "..\..\build-win64\0xSYNTH.clap"
SectionEnd

Section "VST3 Plugin" SecVST3
    SetOutPath "$COMMONFILES64\VST3\0xSYNTH.vst3\Contents\x86_64-win"
    File "..\..\build-win64\0xSYNTH.vst3"
SectionEnd

; --- Descriptions ---
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "0xSYNTH standalone synthesizer with 70 factory presets."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecCLAP} "CLAP plugin for DAWs (installs to Common Files\CLAP)."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} "VST3 plugin for DAWs (installs to Common Files\VST3)."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; --- Uninstaller ---

Section "Uninstall"
    Delete "$INSTDIR\0xsynth.exe"
    Delete "$INSTDIR\README.md"
    Delete "$INSTDIR\LICENSE"
    Delete "$INSTDIR\FEATURES.md"
    Delete "$INSTDIR\uninstall.exe"

    RMDir /r "$INSTDIR\presets"
    RMDir "$INSTDIR"

    ; Plugins
    Delete "$COMMONFILES64\CLAP\0xSYNTH.clap"
    Delete "$COMMONFILES64\VST3\0xSYNTH.vst3\Contents\x86_64-win\0xSYNTH.vst3"
    RMDir "$COMMONFILES64\VST3\0xSYNTH.vst3\Contents\x86_64-win"
    RMDir "$COMMONFILES64\VST3\0xSYNTH.vst3\Contents"
    RMDir "$COMMONFILES64\VST3\0xSYNTH.vst3"

    ; Shortcuts
    Delete "$SMPROGRAMS\0xSYNTH\0xSYNTH.lnk"
    Delete "$SMPROGRAMS\0xSYNTH\Uninstall.lnk"
    RMDir "$SMPROGRAMS\0xSYNTH"
    Delete "$DESKTOP\0xSYNTH.lnk"

    ; Registry
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\0xSYNTH"
    DeleteRegKey HKLM "Software\0xSYNTH"
SectionEnd
