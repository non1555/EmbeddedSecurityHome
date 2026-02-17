@echo off
setlocal

set "ROOT=%~dp0"
set "BOOTSTRAP=%ROOT%tools\line_bridge\bootstrap.cmd"
set "ENV_FILE=%ROOT%tools\line_bridge\.env"
set "NGROK_BUNDLED=%ROOT%tools\ngrok\ngrok.exe"
set "HAS_PIO=0"
set "HAS_NGROK=0"

echo [1/4] Bootstrap bridge environment...
if not exist "%BOOTSTRAP%" (
  echo ERROR: Missing bootstrap script: %BOOTSTRAP%
  exit /b 2
)
call "%BOOTSTRAP%" --prepare-only
if %ERRORLEVEL% NEQ 0 (
  echo ERROR: Bridge bootstrap failed.
  exit /b 2
)

echo [2/4] Check PlatformIO...
where platformio >nul 2>&1 && set "HAS_PIO=1"
if "%HAS_PIO%"=="0" where pio >nul 2>&1 && set "HAS_PIO=1"
if "%HAS_PIO%"=="0" (
  where py >nul 2>&1
  if %ERRORLEVEL% EQU 0 (
    py -3 -m platformio --version >nul 2>&1 && set "HAS_PIO=1"
    if "%HAS_PIO%"=="0" (
      echo Installing PlatformIO via pip...
      py -3 -m pip install --upgrade platformio
      py -3 -m platformio --version >nul 2>&1 && set "HAS_PIO=1"
    )
  )
)
if "%HAS_PIO%"=="0" (
  where python >nul 2>&1
  if %ERRORLEVEL% EQU 0 (
    python -m platformio --version >nul 2>&1 && set "HAS_PIO=1"
    if "%HAS_PIO%"=="0" (
      echo Installing PlatformIO via pip...
      python -m pip install --upgrade platformio
      python -m platformio --version >nul 2>&1 && set "HAS_PIO=1"
    )
  )
)
if "%HAS_PIO%"=="0" (
  powershell -NoProfile -Command "python -m platformio --version > $null 2>&1; if($LASTEXITCODE -eq 0){ exit 0 } else { exit 1 }"
  if %ERRORLEVEL% EQU 0 (
    set "HAS_PIO=1"
  ) else (
    powershell -NoProfile -Command "python -m pip install --upgrade platformio; python -m platformio --version > $null 2>&1; if($LASTEXITCODE -eq 0){ exit 0 } else { exit 1 }"
    if %ERRORLEVEL% EQU 0 set "HAS_PIO=1"
  )
)

echo [3/4] Check ngrok...
where ngrok >nul 2>&1 && set "HAS_NGROK=1"
if "%HAS_NGROK%"=="0" (
  if exist "%NGROK_BUNDLED%" set "HAS_NGROK=1"
)

echo [4/4] Verify required local files...
if exist "%ENV_FILE%" (
  set "HAS_ENV=1"
) else (
  set "HAS_ENV=0"
)

echo.
echo ===== Setup Summary =====
if "%HAS_PIO%"=="1" (
  echo PlatformIO: OK
) else (
  echo PlatformIO: MISSING ^(install Python then run: python -m pip install platformio^)
)
if "%HAS_NGROK%"=="1" (
  echo ngrok: OK
) else (
  echo ngrok: MISSING ^(install from https://ngrok.com/downloads^)
)
if "%HAS_ENV%"=="1" (
  echo tools\line_bridge\.env: OK
) else (
  echo tools\line_bridge\.env: MISSING
)
echo.
echo Next:
echo 1^) Fill LINE/ngrok values in tools\line_bridge\.env
echo 2^) Launch UI: tools\line_bridge\launcher.vbs
echo 3^) Firmware build: python -m platformio run

if "%HAS_PIO%"=="0" exit /b 1
if "%HAS_NGROK%"=="0" exit /b 1
exit /b 0
