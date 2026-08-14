@echo off
setlocal
chcp 65001 >nul 2>nul

set "BUILD_ARG="
if /I "%~1"=="rebuild" set "BUILD_ARG=rebuild"
if not "%~1"=="" if /I not "%~1"=="rebuild" (
    echo ERROR: Unknown argument "%~1". Usage: %~nx0 [rebuild]
    exit /b 2
)

set "SIMPLEFONTHOOK_NO_PAUSE=1"
set "BUILD_RESULT=1"

call "%~dp0build_x32.bat" %BUILD_ARG%
set "BUILD_RESULT=%ERRORLEVEL%"
if not "%BUILD_RESULT%"=="0" goto :finished

call "%~dp0build_x64.bat" %BUILD_ARG%
set "BUILD_RESULT=%ERRORLEVEL%"

:finished
echo.
echo ============================================
if "%BUILD_RESULT%"=="0" (
    echo Win32 and x64 Release builds completed.
    echo Compressed DLLs: Release\winmm.dll and x64\Release\winmm.dll
) else (
    echo Release build failed with errorlevel: %BUILD_RESULT%
)
echo ============================================
pause
exit /b %BUILD_RESULT%
