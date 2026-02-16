@echo off
echo Cleaning build files and outputs...

if exist *.o del /Q *.o
if exist anc_system.exe del /Q anc_system.exe
if exist result\*.txt del /Q result\*.txt
if exist result\*.wav del /Q result\*.wav

echo Clean complete!
pause
