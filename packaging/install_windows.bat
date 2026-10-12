@echo off
setlocal
title PYRA - Installazione / Install

rem ---- chiede i permessi di amministratore (serve per Program Files) ----
set "ISADMIN="
net session >nul 2>&1 && set "ISADMIN=1"
fltmc >nul 2>&1 && set "ISADMIN=1"
if not defined ISADMIN (
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
set "OLDDLL=%DEST%\PYRA.vst3\Contents\x86_64-win\PYRA.vst3"

rem ---- the old PYRA can't be replaced while a program has it open: FL Studio, ilbridge, other DAWs ----
rem      set PYRA_AUTOCLOSE=1 to close them without asking
set "TRIES=0"
:check_busy
set "BUSY="
set "PIDS="
if not exist "%OLDDLL%" goto remove_old
for /f "usebackq tokens=1*" %%A in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$d=$env:OLDDLL; Get-Process | Where-Object { try { $_.Modules.FileName -contains $d } catch { $false } } | ForEach-Object { '' + $_.Id + ' ' + $_.ProcessName + '.exe' }"`) do (
    call set "PIDS=%%PIDS%% %%A"
    call set "BUSY=%%BUSY%%  %%B"
)
if not defined PIDS goto remove_old
set /a TRIES+=1
if %TRIES% GTR 4 goto remove_old
echo.
echo [!] PYRA e' ancora aperto in:%BUSY%
echo     Salva il tuo progetto: per aggiornare PYRA questi programmi vanno chiusi.
echo [!] PYRA is still open in:%BUSY% - save your project, they need to be closed.
echo.
if defined PYRA_AUTOCLOSE goto kill_busy
choice /C SN /M "Li chiudo io adesso? S = si', N = no, li chiudo da solo"
if errorlevel 2 (
    echo Chiudili ^(anche dall'icona in basso a destra vicino all'orologio^), poi premi un tasto...
    pause >nul
    goto check_busy
)
:kill_busy
for %%P in (%PIDS%) do taskkill /F /PID %%P >nul 2>&1
ping -n 3 127.0.0.1 >nul
goto check_busy

:remove_old
if exist "%DEST%\PYRA.vst3" (
    echo Rimuovo la versione precedente di PYRA...
    rmdir /s /q "%DEST%\PYRA.vst3" >nul 2>&1
)
rem still locked by something we could not see, like an antivirus: a file in use can usually still be renamed
if exist "%OLDDLL%" (
    ren "%OLDDLL%" "PYRA.vst3.old%RANDOM%" >nul 2>&1
)
if exist "%OLDDLL%" (
    echo.
    echo [ERRORE] Windows tiene ancora bloccato il vecchio PYRA.
    echo Riavvia il computer e lancia di nuovo install_windows.bat PRIMA di aprire FL Studio.
    echo [ERROR] The old PYRA is still locked. Restart the computer and run this installer again
    echo before opening your DAW.
    echo.
    pause
    exit /b 1
)

echo Copio PYRA.vst3 in "%DEST%" ^(con i suoni campionati, ci vuole qualche secondo^)...
robocopy "%SRC%" "%DEST%\PYRA.vst3" /E /R:3 /W:2 /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 (
    echo.
    echo [ERRORE] Copia non riuscita. Chiudi FL Studio / la DAW ^(o riavvia il computer^) e riprova.
    echo [ERROR] Copy failed. Close your DAW ^(or restart the computer^) and try again.
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
