@echo off
setlocal

if "%BUILD_DIR%"=="" set BUILD_DIR=build

if not exist "%BUILD_DIR%" (
    echo Build directory '%BUILD_DIR%' not found — run build.bat first. 1>&2
    exit /b 1
)

ctest --test-dir "%BUILD_DIR%" --output-on-failure --parallel %NUMBER_OF_PROCESSORS%
