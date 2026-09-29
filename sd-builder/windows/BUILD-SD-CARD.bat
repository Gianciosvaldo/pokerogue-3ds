@echo off
rem PokeRogue 3DS SD card builder: double-click to run. See READ-ME-FIRST.txt.
setlocal
title PokeRogue SD card builder
cd /d "%~dp0"

if not exist "%~dp0files\builder.py" (
  echo.
  echo  This window was started from inside the zip file, so the builder can't find its files.
  echo.
  echo  Close this window. Then right-click PokeRogue-SD-Builder.zip, choose "Extract All...",
  echo  click "Extract", and double-click BUILD-SD-CARD in the folder that opens.
  echo.
  pause
  exit /b 1
)

set RC=0
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0files\setup.ps1"
if errorlevel 1 (
  set RC=1
  goto failed
)

"%~dp0files\runtime\python\python.exe" -X utf8 "%~dp0files\builder.py" %*
set RC=%errorlevel%
if not "%RC%"=="0" goto failed
goto done

:failed
echo.
echo  The builder stopped before it finished. The message above says why.
echo  Everything it did so far is kept: double-click BUILD-SD-CARD again to carry on.
echo  If it keeps failing, send build-log.txt (in this folder) in the project thread.

:done
echo.
pause
exit /b %RC%
