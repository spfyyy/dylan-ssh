@echo off
setlocal
cls

set PROJ_DIR=%~dp0.
cd "%PROJ_DIR%"

REM set up windows build environment
if "%VS_PATH%" == "" (
    echo "VS_PATH environment variable not defined" >&2
)
cl >NUL 2>NUL || call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" || goto :error

REM set up build folder
if not exist build (
    mkdir build || goto :error
)
cd build

cl /Zi "%PROJ_DIR%\windows_ssh.c" || goto :error
goto :EOF

:error
exit /b 1
