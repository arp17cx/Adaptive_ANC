@echo off
echo ============================================
echo   Building ANC System (Organized Structure)
echo ============================================
echo.

where gcc >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: gcc not found!
    echo Please install TDM-GCC from: https://jmeubank.github.io/tdm-gcc/
    pause
    exit /b 1
)

echo Creating result directory...
if not exist result mkdir result

echo [1/6] Compiling src/wav_io.c...
gcc -c src/wav_io.c -o wav_io.o -Iinc -Wall -O2
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile wav_io.c
    pause
    exit /b 1
)

echo [2/6] Compiling src/fir_filter.c...
gcc -c src/fir_filter.c -o fir_filter.o -Iinc -Wall -O2
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile fir_filter.c
    pause
    exit /b 1
)

echo [3/6] Compiling src/time_domain_sim.c...
gcc -c src/time_domain_sim.c -o time_domain_sim.o -Iinc -Wall -O2
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile time_domain_sim.c
    pause
    exit /b 1
)

echo [4/6] Compiling src/logger.c...
gcc -c src/logger.c -o logger.o -Iinc -Wall -O2
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile logger.c
    pause
    exit /b 1
)

echo [5/6] Compiling src/main.c...
gcc -c src/main.c -o main.o -Iinc -Wall -O2
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile main.c
    pause
    exit /b 1
)

echo [6/6] Linking...
gcc main.o wav_io.o fir_filter.o time_domain_sim.o logger.o -o anc_system.exe -lm
if %errorlevel% neq 0 (
    echo ERROR: Failed to link
    pause
    exit /b 1
)

echo.
echo ============================================
echo   Build Successful!
echo   Executable: anc_system.exe
echo ============================================
echo.
echo Output files will be in: result/
echo   - result/anc_log.txt
echo   - result/output_comparison.wav
echo.

pause
