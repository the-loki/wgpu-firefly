@echo off
REM Firefly build script - sets up MSVC environment and runs CMake
REM Usage: build.bat [configure|build|test|clean] [debug|release]

setlocal enabledelayedexpansion

set "VCVARSALL="
for %%v in (2022 2019) do (
    for %%e in (Community Professional Enterprise) do (
        set "CANDIDATE=C:\Program Files\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat"
        if exist "!CANDIDATE!" (
            set "VCVARSALL=!CANDIDATE!"
            goto :found_vcvars
        )
    )
)
echo ERROR: Could not find vcvarsall.bat
exit /b 1

:found_vcvars
call "%VCVARSALL%" x64 >nul 2>&1

set "ACTION=%~1"
set "CONFIG=%~2"
if "%ACTION%"=="" set "ACTION=build"
if "%CONFIG%"=="" set "CONFIG=debug"

if "%CONFIG%"=="release" (
    set "PRESET=release"
) else (
    set "PRESET=default"
)

if "%ACTION%"=="configure" goto :configure
if "%ACTION%"=="build" goto :build
if "%ACTION%"=="test" goto :test
if "%ACTION%"=="clean" goto :clean
echo Unknown action: %ACTION%
exit /b 1

:configure
cmake --preset=%PRESET%
exit /b %ERRORLEVEL%

:build
cmake --preset=%PRESET% && cmake --build --preset=%CONFIG%
exit /b %ERRORLEVEL%

:test
cmake --preset=%PRESET% && cmake --build --preset=%CONFIG% && ctest --test-dir build\%PRESET% --output-on-failure
exit /b %ERRORLEVEL%

:clean
if exist build\%PRESET% rmdir /s /q build\%PRESET%
exit /b 0
