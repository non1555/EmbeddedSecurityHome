@echo off
setlocal enabledelayedexpansion

REM Stop bridge + ngrok (best-effort).

set "ROOT=%~dp0"
set "ENV_FILE=%ROOT%.env"
set "HTTP_PORT=8080"

if exist "%ENV_FILE%" (
  for /f "usebackq tokens=1,2 delims==" %%A in ("%ENV_FILE%") do (
    if /I "%%A"=="HTTP_PORT" set "HTTP_PORT=%%B"
  )
)

echo Stopping bridge on port %HTTP_PORT%...
for /f "tokens=5" %%P in ('netstat -ano ^| findstr ":%HTTP_PORT% " ^| findstr "LISTENING"') do (
  echo Killing PID %%P ...
  taskkill /PID %%P /F >nul 2>&1
)

echo Stopping ngrok...
taskkill /IM ngrok.exe /F >nul 2>&1

echo Stopped.
exit /b 0
