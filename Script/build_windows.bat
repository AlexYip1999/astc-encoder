@echo off
REM Build script for astc-encoder on Windows

REM Set up build directory
if not exist "..\build" (
    mkdir "..\build"
)

cd "..\build"

REM Run CMake to generate Visual Studio solution
cmake .. -G "Visual Studio 17 2022"

cd ..
echo Build complete.
