@echo off
echo ============================================
echo   ANC System - One-Click Run
echo ============================================
echo.

if not exist anc_system.exe (
    echo Building first...
    call build.bat
    if %errorlevel% neq 0 (
        echo Build failed!
        pause
        exit /b 1
    )
)

echo Running ANC System...
echo.

anc_system.exe

echo.
echo ============================================
echo   Execution Completed
echo ============================================
echo.
echo Check result folder for outputs:
if exist result\anc_log.txt (
    echo   [OK] result\anc_log.txt
)
if exist result\output_comparison.wav (
    echo   [OK] result\output_comparison.wav
)
echo.

pause
