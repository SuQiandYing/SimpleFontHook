@echo off
setlocal enabledelayedexpansion

set "BUILD_TARGET=Build"
if /I "%~1"=="rebuild" set "BUILD_TARGET=Rebuild"
if not "%~1"=="" if /I not "%~1"=="rebuild" (
    echo ERROR: Unknown argument "%~1". Usage: %~nx0 [rebuild]
    exit /b 2
)

REM --- 1. Find vswhere.exe ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
)

set "VS_PATH="

REM --- 2. Try to find VS Path using vswhere ---
if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_PATH=%%i"
    )
)

REM --- 3. Fallback: Check common paths manually ---
if not defined VS_PATH (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
)
if not defined VS_PATH (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
)
if not defined VS_PATH (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat" set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
)

REM --- 4. Check if VS was found ---
if not defined VS_PATH (
    echo ERROR: Visual Studio path not found.
    pause
    exit /b 1
)

echo Found VS at: !VS_PATH!

REM --- 5. Call 32-bit environment variables ---
call "!VS_PATH!\VC\Auxiliary\Build\vcvars32.bat"

REM --- 6. Build ---
cd /d "%~dp0"

REM Suppress LNK4099 (Missing PDB for detours.lib)
set "LINK=/IGNORE:4099"

msbuild SimpleFontHook.sln /p:Configuration=Release /p:Platform=Win32 /t:!BUILD_TARGET! /v:minimal
set "BUILD_RESULT=!ERRORLEVEL!"

if !BUILD_RESULT! NEQ 0 (
    echo.
    echo Build failed with Platform=Win32. Trying Platform=x86...
    msbuild SimpleFontHook.sln /p:Configuration=Release /p:Platform=x86 /t:!BUILD_TARGET! /v:minimal
    set "BUILD_RESULT=!ERRORLEVEL!"
)

echo.
echo ============================================
echo Build target: !BUILD_TARGET!
echo Build finished with errorlevel: !BUILD_RESULT!
echo ============================================
pause
exit /b !BUILD_RESULT!
