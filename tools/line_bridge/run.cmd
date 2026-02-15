@echo off
setlocal enabledelayedexpansion

REM Run LINE bridge + ngrok in a user-friendly way.
REM - Uses local venv Python (avoids WindowsApps python without deps)
REM - Reads HTTP_PORT from .env
REM - Prints ngrok public URL + webhook URL and copies it to clipboard

set "ROOT=%~dp0"
set "PY=%ROOT%.venv\Scripts\python.exe"
set "ENV_FILE=%ROOT%.env"
set "ENV_EXAMPLE=%ROOT%.env.example"
set "HTTP_PORT=8080"
set "NGROK_URL="

if not exist "%PY%" (
  echo ERROR: venv python not found: "%PY%"
  echo Create venv in tools\line_bridge and install requirements.txt first:
  echo   cd tools\line_bridge
  echo   python -m venv .venv
  echo   .venv\Scripts\python.exe -m pip install -r requirements.txt
  exit /b 2
)

if not exist "%ENV_FILE%" (
  if exist "%ENV_EXAMPLE%" (
    echo Creating .env from .env.example...
    copy /Y "%ENV_EXAMPLE%" "%ENV_FILE%" >nul
  )
  echo Please edit "%ENV_FILE%" then run this again.
  pause
  exit /b 2
)

for /f "usebackq tokens=1,2 delims==" %%A in ("%ENV_FILE%") do (
  if /I "%%A"=="HTTP_PORT" set "HTTP_PORT=%%B"
)

echo.
echo EmbeddedSecurity LINE Bridge Launcher
echo ====================================
echo Port: %HTTP_PORT%
echo.

REM Quick dependency check (prints a clear message if missing)
"%PY%" -c "import fastapi,uvicorn,requests,paho.mqtt.client as mqtt" >nul 2>&1
if errorlevel 1 (
  echo ERROR: Python dependencies not installed in venv.
  echo Fix:
  echo   cd tools\line_bridge
  echo   .venv\Scripts\python.exe -m pip install -r requirements.txt
  pause
  exit /b 2
)

where ngrok >nul 2>&1
if errorlevel 1 (
  echo ERROR: ngrok not found in PATH.
  echo Install ngrok and ensure `ngrok` is available in your terminal.
  pause
  exit /b 2
)

REM Free port (bridge)
for /f "tokens=5" %%P in ('netstat -ano ^| findstr ":%HTTP_PORT% " ^| findstr "LISTENING"') do (
  echo Port %HTTP_PORT% is in use. Killing PID %%P ...
  taskkill /PID %%P /F >nul 2>&1
)

REM Start bridge (new window)
echo Starting bridge on port %HTTP_PORT%...
pushd "%ROOT%"
start "line_bridge" "%PY%" "%ROOT%bridge.py"
popd

REM Start ngrok (new window)
echo Starting ngrok tunnel...
start "ngrok" ngrok http %HTTP_PORT%

REM Open local pages
start "" "http://127.0.0.1:%HTTP_PORT%/health"
start "" "http://127.0.0.1:4040"

REM Try to fetch ngrok URL and copy webhook URL to clipboard
echo.
echo Waiting for ngrok to be ready...
timeout /t 2 /nobreak >nul

for /f "usebackq delims=" %%U in (`powershell -NoProfile -Command "try { (Invoke-RestMethod 'http://127.0.0.1:4040/api/tunnels').tunnels | Where-Object proto -eq 'https' | Select-Object -First 1 -ExpandProperty public_url } catch { '' }"`) do (
  set "NGROK_URL=%%U"
)

if not "%NGROK_URL%"=="" (
  echo ngrok public URL:
  echo   %NGROK_URL%
  echo LINE webhook URL:
  echo   %NGROK_URL%/line/webhook
  powershell -NoProfile -Command "Set-Clipboard '%NGROK_URL%/line/webhook'" >nul 2>&1
  echo Copied webhook URL to clipboard.
) else (
  echo Could not read ngrok URL from local API.
  echo Open http://127.0.0.1:4040 and copy the https URL manually.
)

echo.
echo Next steps (LINE Developers):
echo 1. Messaging API -> Webhook settings
echo 2. Paste webhook URL (copied)
echo 3. Verify + Enable 'Use webhook'
echo 4. Send 'status' to your OA once (bridge auto-learns push target)
echo.
echo This window can be closed. Bridge and ngrok are running in their own windows.
echo.
pause
exit /b 0
