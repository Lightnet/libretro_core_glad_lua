@echo off
setlocal

set PROJECT_DIR=%CD%
set BUILD_DIR=%PROJECT_DIR%\build
set CONFIG=Debug
set RETROARCH_DIR=D:\dev\RetroArch-Win64
set CORE_NAME=libretro_core_glad_lua.dll
set CONTENT=script.zip
set ZIP="C:\Program Files\7-Zip\7z.exe"
set ROM=assets.zip

@REM compress file
@REM %ZIP% a %ROM% script.lua
%ZIP% a %ROM% ./assets/*

@REM retroarch.exe -L libretro_core_glad_lua.dll script.zip
%RETROARCH_DIR%/retroarch -L %BUILD_DIR%/%CONFIG%/%CORE_NAME% %ROM%

endlocal