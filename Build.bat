@echo off

cd installer
mkdir data 2>/nul
cd ../dsrom
echo.
echo Building DS ROMs
echo.
make clean
make

cd ../installer
echo.
echo Building WiiU Installer
echo.
make clean
make

echo.
pause
