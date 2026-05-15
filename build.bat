@echo off
setlocal

if "%BUILD_DIR%"=="" set BUILD_DIR=build
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Debug

if exist "%BUILD_DIR%\CMakeCache.txt" del /f /q "%BUILD_DIR%\CMakeCache.txt"
if exist "%BUILD_DIR%\CMakeFiles" rmdir /s /q "%BUILD_DIR%\CMakeFiles"

cmake -S . -B "%BUILD_DIR%"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel %NUMBER_OF_PROCESSORS%
