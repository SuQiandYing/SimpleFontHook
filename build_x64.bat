@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>nul

echo Searching for Visual Studio...

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
)

set "VS_PATH="
if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_PATH=%%i"
    )
)

if not defined VS_PATH (
    REM Fallback
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
)

if not defined VS_PATH (
    echo ERROR: Visual Studio path not found.
    pause
    exit /b 1
)

echo Found VS at: !VS_PATH!

REM Suppress LNK4099 (Missing PDB for detours.lib)
set "LINK=/IGNORE:4099"

call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat"

cd /d "%~dp0"
msbuild SimpleFontHook.sln /p:Configuration=Release /p:Platform=x64 /t:Rebuild /v:minimal

echo.
echo ============================================
echo Build finished with errorlevel: %ERRORLEVEL%
echo ============================================
pause
