@echo off
setlocal
title PYRA - Installazione / Install

rem ---- chiede i permessi di amministratore (serve per Program Files) ----
net session >nul 2>&1
if errorlevel 1 (
    echo Richiesta permessi di amministratore... / Asking for administrator rights...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

set "SRC=%~dp0PYRA.vst3"
set "DEST=%CommonProgramFiles%\VST3"

echo.
echo ================================================
echo   PYRA - installazione su Windows
echo ================================================
echo.

if not exist "%SRC%\Contents" (
    echo [ERRORE] Non trovo la cartella PYRA.vst3 accanto a questo file.
    echo.
    echo Probabilmente hai aperto il file direttamente dentro lo ZIP.
    echo Fai cosi': tasto destro su PYRA-Windows.zip ^> "Estrai tutto...",
    echo poi apri la cartella estratta e fai doppio clic su install_windows.bat.
    echo.
    echo [ERROR] PYRA.vst3 not found next to this file. Extract the zip first
    echo ^(right click ^> Extract All^), then run install_windows.bat from the extracted folder.
    echo.
    pause
    exit /b 1
)

echo Sblocco i file scaricati da internet...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem -LiteralPath '%~dp0' -Recurse | Unblock-File" >nul 2>&1

if not exist "%DEST%" mkdir "%DEST%"
if exist "%DEST%\PYRA.vst3" (
    echo Rimuovo la versione precedente di PYRA...
    rmdir /s /q "%DEST%\PYRA.vst3"
)

echo Copio PYRA.vst3 in "%DEST%" ^(con i suoni campionati, ci vuole qualche secondo^)...
robocopy "%SRC%" "%DEST%\PYRA.vst3" /E /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 (
    echo.
    echo [ERRORE] Copia non riuscita. Chiudi FL Studio / la DAW e riprova.
    echo [ERROR] Copy failed. Close your DAW and try again.
    pause
    exit /b 1
)

if exist "%~dp0PYRA.exe" (
    if not exist "%ProgramFiles%\PYRA" mkdir "%ProgramFiles%\PYRA"
    copy /y "%~dp0PYRA.exe" "%ProgramFiles%\PYRA\PYRA.exe" >nul
    powershell -NoProfile -ExecutionPolicy Bypass -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath('CommonDesktopDirectory')+'\PYRA.lnk'); $s.TargetPath='%ProgramFiles%\PYRA\PYRA.exe'; $s.Save()" >nul 2>&1
)

echo.
echo ================================================
echo   FATTO! PYRA e' installato in:
echo   %DEST%\PYRA.vst3
echo ================================================
echo.
echo Ora in FL Studio:
echo   Options ^> Manage plugins ^> "Find installed plugins"
echo   poi cerca PYRA nella lista e clicca la stellina.
echo.
echo In Ableton: Impostazioni ^> Plug-in ^> "Usa cartella VST3 di sistema" ^> Rescan.
echo.
echo DONE! In FL Studio: Options ^> Manage plugins ^> Find installed plugins.
echo.
pause
