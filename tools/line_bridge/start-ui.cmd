@echo off
setlocal

set "ROOT=%~dp0"
set "BOOTSTRAP=%ROOT%bootstrap.cmd"
set "PYW=%ROOT%.venv\Scripts\pythonw.exe"
set "PY=%ROOT%.venv\Scripts\python.exe"
set "SCRIPT=%ROOT%launcher.pyw"

if not exist "%BOOTSTRAP%" (
  echo ERROR: Missing bootstrap script: %BOOTSTRAP%
  exit /b 2
)

call "%BOOTSTRAP%" --prepare-only
if %ERRORLEVEL% NEQ 0 (
  echo ERROR: bootstrap failed.
  exit /b 2
)

if exist "%PYW%" (
  start "" "%PYW%" "%SCRIPT%"
  exit /b 0
)

if exist "%PY%" (
  start "" "%PY%" "%SCRIPT%"
  exit /b 0
)

echo ERROR: venv python not found after bootstrap.
exit /b 2
