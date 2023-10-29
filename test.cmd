@echo off
setlocal
cls

set PROJ_DIR=%~dp0.
cd "%PROJ_DIR%"

.\build\windows_ssh.exe
