@echo off
setlocal

if "%BUILD_DIR%"=="" set BUILD_DIR=build
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Debug

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    cmake -S . -B "%BUILD_DIR%"
    if errorlevel 1 exit /b 1
)

cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel %NUMBER_OF_PROCESSORS%
