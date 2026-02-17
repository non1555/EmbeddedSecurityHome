@echo off
setlocal

set "ROOT=%~dp0"
set "PREP_ONLY=0"
if /I "%~1"=="--prepare-only" set "PREP_ONLY=1"

set "ENV_FILE=%ROOT%.env"
set "ENV_EXAMPLE=%ROOT%.env.example"
if not exist "%ENV_FILE%" (
  if exist "%ENV_EXAMPLE%" (
    echo Creating .env from .env.example...
    copy /Y "%ENV_EXAMPLE%" "%ENV_FILE%" >nul
  )
)

set "PY="
where py >nul 2>&1 && set "PY=py -3"
if not defined PY where python >nul 2>&1 && set "PY=python"
if not defined PY (
  echo ERROR: Python not found. Install Python 3.10+ first.
  exit /b 2
)

set "VPY=%ROOT%.venv\Scripts\python.exe"
if not exist "%VPY%" (
  echo Creating virtual environment...
  %PY% -m venv "%ROOT%.venv"
  if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to create .venv
    exit /b 2
  )
)

echo Installing bridge dependencies...
"%VPY%" -m pip install --upgrade pip
if %ERRORLEVEL% NEQ 0 exit /b 2
"%VPY%" -m pip install -r "%ROOT%requirements.txt"
if %ERRORLEVEL% NEQ 0 exit /b 2

set "NGROK_OK=0"
where ngrok >nul 2>&1
if %ERRORLEVEL% EQU 0 set "NGROK_OK=1"

if "%NGROK_OK%"=="0" (
  if exist "%ROOT%..\ngrok\ngrok.exe" (
    set "PATH=%ROOT%..\ngrok;%PATH%"
    set "NGROK_OK=1"
  )
)

if "%NGROK_OK%"=="0" (
  where winget >nul 2>&1
  if %ERRORLEVEL% EQU 0 (
    echo Installing ngrok with winget...
    winget install -e --id Ngrok.Ngrok --accept-source-agreements --accept-package-agreements
    where ngrok >nul 2>&1
    if %ERRORLEVEL% EQU 0 set "NGROK_OK=1"
  )
)

if "%NGROK_OK%"=="0" (
  echo WARN: ngrok not found yet. Install manually from https://ngrok.com/downloads
)

if "%PREP_ONLY%"=="1" exit /b 0

set "PYW=%ROOT%.venv\Scripts\pythonw.exe"
if exist "%PYW%" (
  start "" "%PYW%" "%ROOT%launcher.pyw"
  exit /b 0
)

start "" "%VPY%" "%ROOT%launcher.pyw"
exit /b 0
