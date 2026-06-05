@echo off
:: Wait 1 second to allow the caller to exit gracefully
ping 127.0.0.1 -n 2 > nul

taskkill /F /IM sara_agent.exe /T >nul 2>&1
taskkill /F /IM cloudflared.exe /T >nul 2>&1
taskkill /F /IM filebrowser.exe /T >nul 2>&1

start "" "%~dp0..\build\sara_agent.exe"

exit
