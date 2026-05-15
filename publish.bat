@echo off
setlocal

set DIST_NAME=arena_allocator
set BUILD_DIR=build_publish
set STAGE=_publish_stage
set ZIP_FILE=%DIST_NAME%.zip

echo =^> Building release...
cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
if errorlevel 1 exit /b 1
cmake --build "%BUILD_DIR%" --target arena --config Release --parallel %NUMBER_OF_PROCESSORS%
if errorlevel 1 exit /b 1

echo =^> Staging...
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\%DIST_NAME%\lib"
mkdir "%STAGE%\%DIST_NAME%\include"
mkdir "%STAGE%\%DIST_NAME%\docs"

rem MSVC multi-config generator puts the lib under Release/
if exist "%BUILD_DIR%\Release\arena.lib" (
    copy "%BUILD_DIR%\Release\arena.lib" "%STAGE%\%DIST_NAME%\lib\"
) else if exist "%BUILD_DIR%\arena.lib" (
    copy "%BUILD_DIR%\arena.lib" "%STAGE%\%DIST_NAME%\lib\"
) else if exist "%BUILD_DIR%\libarena.a" (
    copy "%BUILD_DIR%\libarena.a" "%STAGE%\%DIST_NAME%\lib\"
) else (
    echo ERROR: library not found in %BUILD_DIR% 1>&2
    exit /b 1
)

xcopy /e /i /q "include\arena" "%STAGE%\%DIST_NAME%\include\arena\"
xcopy /q "docs\*.md" "%STAGE%\%DIST_NAME%\docs\"
copy "README.md" "%STAGE%\%DIST_NAME%\"

echo =^> Creating %ZIP_FILE%...
if exist "%ZIP_FILE%" del "%ZIP_FILE%"
powershell -NoProfile -Command ^
    "Compress-Archive -Path '%STAGE%\%DIST_NAME%' -DestinationPath '%CD%\%ZIP_FILE%'"
if errorlevel 1 exit /b 1

rmdir /s /q "%STAGE%"

echo =^> Done: %ZIP_FILE%
endlocal
