@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>nul

cd /d "%~dp0"

if /I "%SIMPLEFONTHOOK_SKIP_PACK%"=="1" (
    echo Release compression skipped because SIMPLEFONTHOOK_SKIP_PACK=1.
    exit /b 0
)

set "PACK_SCOPE=all"
set "UPX_EXE="
if /I "%~1"=="Win32" (
    set "PACK_SCOPE=Win32"
    set "UPX_EXE=%~2"
) else if /I "%~1"=="x64" (
    set "PACK_SCOPE=x64"
    set "UPX_EXE=%~2"
) else if /I "%~1"=="all" (
    set "UPX_EXE=%~2"
) else (
    REM Backward-compatible form: pack_release.bat path-to-upx.exe
    set "UPX_EXE=%~1"
)

if not defined UPX_EXE if exist "%~dp0tools\upx\upx.exe" set "UPX_EXE=%~dp0tools\upx\upx.exe"
if not defined UPX_EXE if defined UPX_PATH set "UPX_EXE=%UPX_PATH%"
if not defined UPX_EXE for %%I in (upx.exe) do set "UPX_EXE=%%~$PATH:I"

if not defined UPX_EXE (
    echo ERROR: upx.exe not found.
    echo Usage: %~nx0 [Win32^|x64^|all] [path-to-upx.exe]
    exit /b 2
)
if not exist "!UPX_EXE!" (
    echo ERROR: UPX does not exist: !UPX_EXE!
    exit /b 2
)

set "PACK_WIN32=0"
set "PACK_X64=0"
if /I "!PACK_SCOPE!"=="all" set "PACK_WIN32=1"
if /I "!PACK_SCOPE!"=="all" set "PACK_X64=1"
if /I "!PACK_SCOPE!"=="Win32" set "PACK_WIN32=1"
if /I "!PACK_SCOPE!"=="x64" set "PACK_X64=1"

if "!PACK_WIN32!"=="1" if not exist "Release\winmm.dll" (
    echo ERROR: Release\winmm.dll does not exist. Build Win32 Release first.
    exit /b 3
)
if "!PACK_X64!"=="1" if not exist "x64\Release\winmm.dll" (
    echo ERROR: x64\Release\winmm.dll does not exist. Build x64 Release first.
    exit /b 3
)

set "STAGE=%TEMP%\SimpleFontHook-compact-!RANDOM!-!RANDOM!"
set "RESULT=1"
mkdir "!STAGE!" >nul 2>nul || goto :cleanup

if "!PACK_WIN32!"=="1" (
    call :pack_one "Release\winmm.dll" "Win32"
    if errorlevel 1 goto :cleanup
)
if "!PACK_X64!"=="1" (
    call :pack_one "x64\Release\winmm.dll" "x64"
    if errorlevel 1 goto :cleanup
)

echo.
echo Release compression completed in the standard output directories.
set "RESULT=0"

:cleanup
if exist "!STAGE!" rmdir /s /q "!STAGE!"
if not "!RESULT!"=="0" echo ERROR: Release compression failed; the existing DLL was not replaced.
exit /b !RESULT!

:pack_one
setlocal enabledelayedexpansion
set "TARGET=%~1"
set "ARCH=%~2"

"%UPX_EXE%" -t --no-progress "!TARGET!" >nul 2>nul
if not errorlevel 1 (
    for %%F in ("!TARGET!") do set "PACKED_SIZE=%%~zF"
    echo   !ARCH!: already compressed and valid ^(!PACKED_SIZE! bytes^)
    endlocal & exit /b 0
)

for %%F in ("!TARGET!") do set "SOURCE_SIZE=%%~zF"
mkdir "%STAGE%\!ARCH!" >nul 2>nul
if errorlevel 1 goto :pack_one_failed
copy /y "!TARGET!" "%STAGE%\!ARCH!\winmm.dll" >nul
if errorlevel 1 goto :pack_one_failed

"%UPX_EXE%" --best --lzma --no-progress "%STAGE%\!ARCH!\winmm.dll"
if errorlevel 1 goto :pack_one_failed
"%UPX_EXE%" -t --no-progress "%STAGE%\!ARCH!\winmm.dll"
if errorlevel 1 goto :pack_one_failed

copy /y "%STAGE%\!ARCH!\winmm.dll" "!TARGET!" >nul
if errorlevel 1 goto :pack_one_failed
"%UPX_EXE%" -t --no-progress "!TARGET!" >nul
if errorlevel 1 goto :pack_one_failed

for %%F in ("!TARGET!") do set "PACKED_SIZE=%%~zF"
echo   !ARCH!: !SOURCE_SIZE! -^> !PACKED_SIZE! bytes ^(!TARGET!^)
endlocal & exit /b 0

:pack_one_failed
endlocal & exit /b 1
