@echo off

rem Make the path of the current batch file, the current directory
cd %~dp0
cd ../..

rem If "clean" is given as param, then, remove the build directory
rem This is usefull when you want to regenerate a cmake cache-file.
if exist build if "%~1"=="clean"  (
    echo Will rengenerate the CMakaCache.txt file
    rmdir /S /Q build
)

rem We need a .\build directory, create a new one if it doesn't exist.
if not exist build (
    mkdir build
)

rem Store the WORKSPACE path to give as install-prefix for the cmake script.
set WORKSPACE=%cd%
if exist build (
    pushd build
    if errorlevel == 0 (
        cmake -G "Visual Studio 14 2015 Win64" -D OpenCV_DIR=D:\Develop\Libs\OpenCV_3.2\opencv\build -D CMAKE_INSTALL_PREFIX=%WORKSPACE% ..
    )
    popd
)
