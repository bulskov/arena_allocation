@echo off
setlocal

if "%BUILD_DIR%"=="" set BUILD_DIR=build
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Debug

cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --parallel %NUMBER_OF_PROCESSORS%
