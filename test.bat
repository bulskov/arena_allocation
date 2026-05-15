@echo off
setlocal

if "%BUILD_DIR%"=="" set BUILD_DIR=build
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Debug

if not exist "%BUILD_DIR%" (
    echo Build directory '%BUILD_DIR%' not found — run build.bat first. 1>&2
    exit /b 1
)

ctest --test-dir "%BUILD_DIR%" --build-config %BUILD_TYPE% --output-on-failure --parallel %NUMBER_OF_PROCESSORS%
