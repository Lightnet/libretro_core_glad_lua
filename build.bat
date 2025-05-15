@echo off
setlocal

set PROJECT_DIR=%CD%
set BUILD_DIR=%PROJECT_DIR%\build
set RETROARCH_DIR=D:\dev\RetroArch-Win64
set CORE_NAME=libretro_core_glad_lua.dll

if not exist build mkdir build
cd build
@REM cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_OPENGL=OFF
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_OPENGL=ON
cmake --build . --config Debug

:: Copy the DLL and font to RetroArch cores directory
echo Copying %CORE_NAME% and fonts to RetroArch...
copy "%BUILD_DIR%\Debug\%CORE_NAME%" "%RETROARCH_DIR%\cores\"
if %ERRORLEVEL% neq 0 (
    echo Failed to copy DLL to RetroArch!
    exit /b %ERRORLEVEL%
)

endlocal