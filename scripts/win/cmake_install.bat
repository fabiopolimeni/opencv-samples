@echo off

rem Make the path of the current batch file, the current directory
cd %~dp0
cd ../..

rem If "clean" is given as param, then, remove the bin directory.
if exist bin if "%~1"=="clean"  (
    rmdir /S /Q bin
)

rem The  ./bin directory will be created by the install process
if exist build (
    pushd build
    cmake --build . --clean-first --target install --config Debug
    cmake --build . --clean-first --target install --config Release
    popd
)
